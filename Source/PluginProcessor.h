#pragma once
#include <JuceHeader.h>

class ReverseDelayAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    ReverseDelayAudioProcessor();
    ~ReverseDelayAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameters
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    // === Reverse Delay State ===
    double sr = 44100.0;
    int chunkSamples = 22050; // default ~500 ms at 44.1k
    int fadeSamples = 256;   // updated in prepare/process
    int writeIdx = 0, playIdx = 0;
    bool recA_playB = true;

    juce::AudioBuffer<float> bufA, bufB, playA, playB;

    // Cached param pointers (valid while apvts lives)
    std::atomic<float>* pTimeMs = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pFeedback = nullptr;
    std::atomic<float>* pFadeMs = nullptr;

    // Helpers
    static inline float paramVal(std::atomic<float>* p, float def) noexcept
    {
        return p ? p->load(std::memory_order_relaxed) : def;
    }

    static inline float xfadeGain(int i, int N, bool fadeIn) noexcept
    {
        if (N <= 0) return 1.0f;
        float g = juce::jlimit<int>(0, N, i) / (float)N;
        return fadeIn ? g : (1.0f - g);
    }

    static inline void reverseCopy(const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>& out)
    {
        out.setSize(in.getNumChannels(), in.getNumSamples());
        for (int ch = 0; ch < in.getNumChannels(); ++ch)
        {
            const float* src = in.getReadPointer(ch);
            float* dst = out.getWritePointer(ch);
            const int N = in.getNumSamples();
            for (int i = 0; i < N; ++i)
                dst[i] = src[N - 1 - i];
        }
    }

    void allocateBuffers();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverseDelayAudioProcessor)
};
