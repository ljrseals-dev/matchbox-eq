#include "PluginEditor.h"

//==============================================================================
MatchboxLookAndFeel::MatchboxLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour::fromRGB (255, 160, 40));
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB (26, 26, 28));
    setColour (juce::ComboBox::backgroundColourId,        juce::Colour::fromRGB (26, 26, 28));
    setColour (juce::ComboBox::textColourId,              juce::Colours::white);
    setColour (juce::ComboBox::outlineColourId,           juce::Colour::fromRGB (74, 74, 82));
    setColour (juce::ComboBox::arrowColourId,             juce::Colours::white);
}

void MatchboxLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider&)
{
    auto bounds  = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (8.0f);
    auto radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto angle   = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (centreX - radius + 2.0f, centreY - radius + 3.0f, radius * 2.0f, radius * 2.0f);

    // Dark metal bezel
    g.setColour (juce::Colour::fromRGB (26, 26, 28));
    g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

    // Radial gradient face
    juce::ColourGradient grad (
        juce::Colour::fromRGB (70, 70, 78), centreX - radius * 0.35f, centreY - radius * 0.35f,
        juce::Colour::fromRGB (12, 12, 14), centreX + radius * 0.6f,  centreY + radius * 0.6f, true);
    g.setGradientFill (grad);
    g.fillEllipse (centreX - radius + 3.0f, centreY - radius + 3.0f,
                   (radius - 3.0f) * 2.0f, (radius - 3.0f) * 2.0f);

    // White indicator line
    juce::Path p;
    const float thickness = 3.0f;
    p.addRectangle (-thickness * 0.5f, -radius + 7.0f, thickness, radius * 0.55f);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    g.setColour (juce::Colours::white);
    g.fillPath (p);

    // Amber ring highlight
    g.setColour (juce::Colour::fromRGB (255, 160, 40).withAlpha (0.25f));
    g.drawEllipse (centreX - radius + 1.5f, centreY - radius + 1.5f,
                   (radius - 1.5f) * 2.0f, (radius - 1.5f) * 2.0f, 1.5f);
}

void MatchboxLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
    g.setColour (juce::Colour::fromRGB (26, 26, 28));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour::fromRGB (74, 74, 82));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    juce::Path arrow;
    arrow.addTriangle ((float) width - 16.0f, (float) height * 0.5f - 3.0f,
                       (float) width - 6.0f,  (float) height * 0.5f - 3.0f,
                       (float) width - 11.0f, (float) height * 0.5f + 4.0f);
    g.setColour (juce::Colours::white);
    g.fillPath (arrow);
}

//==============================================================================
MatchboxEQAudioProcessorEditor::MatchboxEQAudioProcessorEditor (MatchboxEQAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
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

MatchboxEQAudioProcessorEditor::~MatchboxEQAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void MatchboxEQAudioProcessorEditor::setupKnob (juce::Slider& slider, juce::Label& label,
                                                const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    label.setFont (juce::Font (13.0f, juce::Font::bold));
    addAndMakeVisible (label);
}

void MatchboxEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Dark brushed panel
    g.fillAll (juce::Colour::fromRGB (44, 44, 48));

    // Outer border
    g.setColour (juce::Colour::fromRGB (74, 74, 82));
    g.drawRect (getLocalBounds(), 1);

    // VU meter bezel + face
    g.setColour (juce::Colour::fromRGB (17, 17, 17));
    g.fillEllipse (315, 45, 170, 170);
    g.setColour (juce::Colour::fromRGB (208, 208, 213));
    g.fillEllipse (325, 55, 150, 150);

    // VU label
    g.setColour (juce::Colours::black);
    g.setFont (14.0f);
    g.drawText ("MATCHBOX VU", 325, 115, 150, 30, juce::Justification::centred);

    // Decorative switches
    const int sy = 230;
    g.setColour (juce::Colour::fromRGB (26, 26, 28));
    g.fillRect (230, sy, 60, 24);
    g.fillRect (310, sy, 60, 24);
    g.fillRect (390, sy, 60, 24);
    g.fillRect (470, sy, 60, 24);
    g.setColour (juce::Colours::white);
    g.setFont (11.0f);
    g.drawText ("AC OFF",   230, sy, 60, 24, juce::Justification::centred);
    g.drawText ("NO LINE2", 310, sy, 60, 24, juce::Justification::centred);
    g.drawText ("PLAY",     390, sy, 60, 24, juce::Justification::centred);
    g.drawText ("RECORD",   470, sy, 60, 24, juce::Justification::centred);

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawText ("MATCHBOX EQ", 0, 8, 800, 36, juce::Justification::centred);
}

void MatchboxEQAudioProcessorEditor::resized()
{
    bandsLbl.setBounds    (170, 50,  60, 20);
    bandsSlider.setBounds (150, 70, 100, 100);

    demonsLbl.setBounds    (570, 50,  60, 20);
    demonsSlider.setBounds (550, 70, 100, 100);

    unisonLbl.setBounds    (70,  270, 100, 20);
    unisonSlider.setBounds (50,  290, 140, 100);

    genreLbl.setBounds (210, 270, 100, 20);
    genreBox.setBounds (210, 310, 100, 24);

    bitLbl.setBounds (350, 270, 100, 20);
    bitBox.setBounds (350, 310, 100, 24);

    outputLbl.setBounds    (490, 270, 100, 20);
    outputSlider.setBounds (470, 290, 140, 100);

    mixLbl.setBounds    (630, 270, 100, 20);
    mixSlider.setBounds (610, 290, 140, 100);
}
