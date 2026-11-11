#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class MatchboxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MatchboxLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
};

class MatchboxEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MatchboxEQAudioProcessorEditor (MatchboxEQAudioProcessor&);
    ~MatchboxEQAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MatchboxEQAudioProcessor& proc;
    MatchboxLookAndFeel lnf;

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
