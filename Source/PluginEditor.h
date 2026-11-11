#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class MatchboxEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MatchboxEQAudioProcessorEditor (MatchboxEQAudioProcessor&);
    ~MatchboxEQAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MatchboxEQAudioProcessor& proc;

    juce::Slider bandsSlider, demonsSlider, unisonSlider, outputSlider, mixSlider;
    juce::ComboBox bitBox, genreBox;

    juce::Label bandsLbl, demonsLbl, unisonLbl, bitLbl, genreLbl, outputLbl, mixLbl;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SA> aBands, aDemons, aUnison, aOutput, aMix;
    std::unique_ptr<CA> aBit, aGenre;

    void setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatchboxEQAudioProcessorEditor)
};
