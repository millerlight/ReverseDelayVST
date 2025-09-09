#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
ReverseDelayAudioProcessorEditor::ReverseDelayAudioProcessorEditor(ReverseDelayAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    auto setupSlider = [](juce::Slider& s, juce::Label& l, const juce::String& name)
        {
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            l.setText(name, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
        };

    setupSlider(timeMsSlider, timeMsLabel, "Time (ms)");
    setupSlider(mixSlider, mixLabel, "Mix");
    setupSlider(feedbackSlider, feedbackLabel, "Feedback");
    setupSlider(fadeMsSlider, fadeMsLabel, "Fade (ms)");

    addAndMakeVisible(timeMsSlider);   addAndMakeVisible(timeMsLabel);
    addAndMakeVisible(mixSlider);      addAndMakeVisible(mixLabel);
    addAndMakeVisible(feedbackSlider); addAndMakeVisible(feedbackLabel);
    addAndMakeVisible(fadeMsSlider);   addAndMakeVisible(fadeMsLabel);

    timeAtt = std::make_unique<Attachment>(processor.apvts, "timeMs", timeMsSlider);
    mixAtt = std::make_unique<Attachment>(processor.apvts, "mix", mixSlider);
    feedbackAtt = std::make_unique<Attachment>(processor.apvts, "feedback", feedbackSlider);
    fadeAtt = std::make_unique<Attachment>(processor.apvts, "fadeMs", fadeMsSlider);

    setSize(440, 220);
}

ReverseDelayAudioProcessorEditor::~ReverseDelayAudioProcessorEditor() = default;

//==============================================================================
void ReverseDelayAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("Reverse Delay", getLocalBounds().removeFromTop(24), juce::Justification::centred, 1);
}

void ReverseDelayAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(12);
    auto topBar = bounds.removeFromTop(24); juce::ignoreUnused(topBar);

    auto row = bounds.removeFromTop(bounds.getHeight());
    auto w = row.getWidth() / 4;

    auto col1 = row.removeFromLeft(w).reduced(6);
    auto col2 = row.removeFromLeft(w).reduced(6);
    auto col3 = row.removeFromLeft(w).reduced(6);
    auto col4 = row.reduced(6);

    timeMsLabel.setBounds(col1.removeFromTop(20));
    timeMsSlider.setBounds(col1);
    mixLabel.setBounds(col2.removeFromTop(20));
    mixSlider.setBounds(col2);
    feedbackLabel.setBounds(col3.removeFromTop(20));
    feedbackSlider.setBounds(col3);
    fadeMsLabel.setBounds(col4.removeFromTop(20));
    fadeMsSlider.setBounds(col4);
}
