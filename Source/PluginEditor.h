#pragma once
#include <JuceHeader.h>
class ReverseDelayAudioProcessor;

class ReverseDelayAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReverseDelayAudioProcessorEditor(ReverseDelayAudioProcessor&);
    ~ReverseDelayAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ReverseDelayAudioProcessor& processor;

    juce::Slider timeMsSlider, mixSlider, feedbackSlider, fadeMsSlider;
    juce::Label  timeMsLabel, mixLabel, feedbackLabel, fadeMsLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> timeAtt, mixAtt, feedbackAtt, fadeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverseDelayAudioProcessorEditor)
};
