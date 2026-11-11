#include "PluginEditor.h"

MatchboxEQAudioProcessorEditor::MatchboxEQAudioProcessorEditor (MatchboxEQAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (800, 400);

    setupKnob (bandsSlider,  bandsLbl,  "BANDS");
    setupKnob (demonsSlider, demonsLbl, "DENONS");
    setupKnob (unisonSlider, unisonLbl, "UNISON");
    setupKnob (outputSlider, outputLbl, "OUTPUT");
    setupKnob (mixSlider,    mixLbl,    "MIX");

    bitBox.addItemList ({ "16", "20", "24" }, 1);
    bitBox.setSelectedId (3);
    bitLbl.setText ("BIT DEPTH", juce::dontSendNotification);
    bitLbl.setJustificationType (juce::Justification::centred);
    bitLbl.setColour (juce::Label::textColourId, juce::Colours::white);
    bitLbl.setFont (juce::Font (14.0f, juce::Font::bold));
    addAndMakeVisible (bitBox);
    addAndMakeVisible (bitLbl);

    genreBox.addItemList ({ "Neo-Classical", "Cinematic Pop", "Jazz/Hip-Hop", "Flat" }, 1);
    genreBox.setSelectedId (1);
    genreLbl.setText ("GENRE", juce::dontSendNotification);
    genreLbl.setJustificationType (juce::Justification::centred);
    genreLbl.setColour (juce::Label::textColourId, juce::Colours::white);
    genreLbl.setFont (juce::Font (14.0f, juce::Font::bold));
    addAndMakeVisible (genreBox);
    addAndMakeVisible (genreLbl);

    aBands  = std::make_unique<SA> (proc.apvts, "askedBands",   bandsSlider);
    aDemons = std::make_unique<SA> (proc.apvts, "demonCount",   demonsSlider);
    aUnison = std::make_unique<SA> (proc.apvts, "unisonWeight", unisonSlider);
    aOutput = std::make_unique<SA> (proc.apvts, "outputGain",   outputSlider);
    aMix    = std::make_unique<SA> (proc.apvts, "mix",          mixSlider);
    aBit    = std::make_unique<CA> (proc.apvts, "bitDepth",     bitBox);
    aGenre  = std::make_unique<CA> (proc.apvts, "genreId",      genreBox);
}

void MatchboxEQAudioProcessorEditor::setupKnob (juce::Slider& slider, juce::Label& label,
                                                const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    slider.setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour::fromRGB (255, 160, 40));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB (26, 26, 28));
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    label.setFont (juce::Font (14.0f, juce::Font::bold));
    addAndMakeVisible (label);
}

void MatchboxEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (44, 44, 48));

    // Simulated VU meter
    g.setColour (juce::Colour::fromRGB (17, 17, 17));
    g.fillEllipse (315, 45, 170, 170);
    g.setColour (juce::Colour::fromRGB (208, 208, 213));
    g.fillEllipse (325, 55, 150, 150);
    g.setColour (juce::Colours::black);
    g.setFont (14.0f);
    g.drawText ("MATCHBOX VU", 325, 120, 150, 30, juce::Justification::centred);

    // Decorative switches
    g.setColour (juce::Colour::fromRGB (26, 26, 28));
    g.fillRect (230, 230, 60, 24);
    g.fillRect (310, 230, 60, 24);
    g.fillRect (390, 230, 60, 24);
    g.fillRect (470, 230, 60, 24);
    g.setColour (juce::Colours::white);
    g.setFont (11.0f);
    g.drawText ("AC OFF",   230, 230, 60, 24, juce::Justification::centred);
    g.drawText ("NO LINE2", 310, 230, 60, 24, juce::Justification::centred);
    g.drawText ("PLAY",     390, 230, 60, 24, juce::Justification::centred);
    g.drawText ("RECORD",   470, 230, 60, 24, juce::Justification::centred);

    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawText ("MATCHBOX EQ", 0, 10, 800, 40, juce::Justification::centred);
}

void MatchboxEQAudioProcessorEditor::resized()
{
    // Top knobs
    bandsLbl.setBounds   (170, 50, 60, 20);
    bandsSlider.setBounds (150, 70, 100, 100);

    demonsLbl.setBounds   (570, 50, 60, 20);
    demonsSlider.setBounds (550, 70, 100, 100);

    // Bottom row
    unisonLbl.setBounds   (70, 270, 100, 20);
    unisonSlider.setBounds (50, 290, 140, 100);

    genreLbl.setBounds (210, 270, 100, 20);
    genreBox.setBounds (210, 310, 100, 24);

    bitLbl.setBounds (350, 270, 100, 20);
    bitBox.setBounds (350, 310, 100, 24);

    outputLbl.setBounds   (490, 270, 100, 20);
    outputSlider.setBounds (470, 290, 140, 100);

    mixLbl.setBounds   (630, 270, 100, 20);
    mixSlider.setBounds (610, 290, 140, 100);
}
