#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
ReverseDelayAudioProcessor::ReverseDelayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#else
    : AudioProcessor(BusesProperties())
#endif
    , apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    // Cache raw parameter pointers
    pTimeMs = apvts.getRawParameterValue("timeMs");
    pMix = apvts.getRawParameterValue("mix");
    pFeedback = apvts.getRawParameterValue("feedback");
    pFadeMs = apvts.getRawParameterValue("fadeMs");
}

ReverseDelayAudioProcessor::~ReverseDelayAudioProcessor() = default;

//==============================================================================
const juce::String ReverseDelayAudioProcessor::getName() const { return JucePlugin_Name; }
bool ReverseDelayAudioProcessor::acceptsMidi() const { return JucePlugin_WantsMidiInput; }
bool ReverseDelayAudioProcessor::producesMidi() const { return JucePlugin_ProducesMidiOutput; }
bool ReverseDelayAudioProcessor::isMidiEffect() const { return JucePlugin_IsMidiEffect; }
double ReverseDelayAudioProcessor::getTailLengthSeconds() const { return paramVal(pTimeMs, 500.0f) / 1000.0; }

int ReverseDelayAudioProcessor::getNumPrograms() { return 1; }
int ReverseDelayAudioProcessor::getCurrentProgram() { return 0; }
void ReverseDelayAudioProcessor::setCurrentProgram(int) {}
const juce::String ReverseDelayAudioProcessor::getProgramName(int) { return {}; }
void ReverseDelayAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void ReverseDelayAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    juce::FloatVectorOperations::disableDenormalisedNumberSupport();
    sr = (sampleRate > 0.0 ? sampleRate : 44100.0);

    // Convert params to samples
    const int desiredChunk = juce::jlimit<int>(256, (int)(4 * sr),
        (int)std::lrint(paramVal(pTimeMs, 500.0f) * 0.001 * sr));

    int desiredFade = juce::jlimit<int>(16, 2048,
        (int)std::lrint(paramVal(pFadeMs, 5.0f) * 0.001 * sr));
    desiredFade = juce::jlimit<int>(1, juce::jmax(1, desiredChunk - 1), desiredFade);

    bool needRealloc = (desiredChunk != chunkSamples);
    chunkSamples = desiredChunk;
    fadeSamples = desiredFade;

    if (needRealloc)
        allocateBuffers();
    else
    {
        bufA.clear(); bufB.clear(); playA.clear(); playB.clear();
    }

    writeIdx = 0;
    playIdx = 0;
    recA_playB = true;

    setLatencySamples(chunkSamples); // report algorithmic latency to host
}

void ReverseDelayAudioProcessor::allocateBuffers()
{
    bufA.setSize(2, chunkSamples); bufA.clear();
    bufB.setSize(2, chunkSamples); bufB.clear();
    playA.setSize(2, chunkSamples); playA.clear();
    playB.setSize(2, chunkSamples); playB.clear();
}

void ReverseDelayAudioProcessor::releaseResources() {}

bool ReverseDelayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // only allow mono or stereo, and the input layout must match the output layout
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif
    return true;
#endif
}

void ReverseDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals; juce::ignoreUnused(midi);

    if (sr <= 0.0) sr = 44100.0;

    // If user changed time/fade while running, re-evaluate quickly (glitch-safe enough for demo)
    const int desiredChunk = juce::jlimit<int>(256, (int)(4 * sr),
        (int)std::lrint(paramVal(pTimeMs, 500.0f) * 0.001 * sr));

    int desiredFade = juce::jlimit<int>(16, 2048,
        (int)std::lrint(paramVal(pFadeMs, 5.0f) * 0.001 * sr));
    desiredFade = juce::jlimit<int>(1, juce::jmax(1, desiredChunk - 1), desiredFade);

    if (desiredChunk != chunkSamples)
    {
        chunkSamples = desiredChunk;
        allocateBuffers();
        writeIdx = playIdx = 0;
        recA_playB = true;
        setLatencySamples(chunkSamples);
    }
    fadeSamples = desiredFade;

    const float mix = juce::jlimit<float>(0.0f, 1.0f, paramVal(pMix, 0.7f));
    const float feedback = juce::jlimit<float>(0.0f, 0.98f, paramVal(pFeedback, 0.35f));

    const int numCh = juce::jmin(2, buffer.getNumChannels());
    const int N = buffer.getNumSamples();

    if (chunkSamples <= 0 || N <= 0 || numCh <= 0)
        return;

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* out = buffer.getWritePointer(ch);
        const float* in = buffer.getReadPointer(ch);

        for (int n = 0; n < N; ++n)
        {
            auto& recBuf = recA_playB ? bufA : bufB;
            auto& playBuf = recA_playB ? playB : playA;

            const int playN = playBuf.getNumSamples();
            const int recN = recBuf.getNumSamples();
            const float input = in[n];

            float fb = 0.0f;
            if (playIdx >= 0 && playIdx < playN)
            {
                const float* p = playBuf.getReadPointer(ch);
                if (p) fb = p[playIdx] * feedback;
            }

            if (writeIdx >= 0 && writeIdx < recN)
            {
                float* w = recBuf.getWritePointer(ch);
                if (w) w[writeIdx] = input + fb;
            }

            float wet = 0.0f;
            if (playIdx >= 0 && playIdx < playN)
            {
                const float* p = playBuf.getReadPointer(ch);
                if (p) wet = p[playIdx];

                if (fadeSamples > 0)
                {
                    if (playIdx < fadeSamples)
                        wet *= xfadeGain(playIdx, fadeSamples, true);
                    else if (playN - playIdx <= fadeSamples)
                        wet *= xfadeGain(juce::jmax(0, playN - playIdx - 1), fadeSamples, false);
                }
            }

            out[n] = (1.0f - mix) * input + mix * wet;

            if (ch == 0)
            {
                ++writeIdx;
                ++playIdx;

                if (writeIdx >= chunkSamples)
                {
                    auto& rec = recA_playB ? bufA : bufB;
                    auto& nextPlay = recA_playB ? playA : playB;

                    reverseCopy(rec, nextPlay);
                    rec.clear();
                    writeIdx = 0;
                }

                if (playIdx >= chunkSamples)
                {
                    playIdx = 0;
                    recA_playB = !recA_playB;
                }
            }
        }
    }

    // Clear any additional channels if host requested >2
    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, N);
}

//==============================================================================
bool ReverseDelayAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* ReverseDelayAudioProcessor::createEditor()
{
    return new ReverseDelayAudioProcessorEditor(*this);
}

//==============================================================================
void ReverseDelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ReverseDelayAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ReverseDelayAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto timeRange = juce::NormalisableRange<float>{ 50.0f, 2000.0f, 1.0f, 0.5f }; // 50..2000 ms, skewed
    params.push_back(std::make_unique<juce::AudioParameterFloat>("timeMs", "Time (ms)", timeRange, 500.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 1.0f, 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("feedback", "Feedback", 0.0f, 0.98f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fadeMs", "Fade (ms)", 2.0f, 30.0f, 5.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
// JUCE plug-in factory
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverseDelayAudioProcessor();
}
