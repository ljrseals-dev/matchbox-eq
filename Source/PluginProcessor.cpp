#include "PluginProcessor.h"
#include "PluginEditor.h"

MatchboxEQAudioProcessor::MatchboxEQAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
MatchboxEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using namespace juce;

    layout.add (std::make_unique<AudioParameterFloat> (
        "askedBands", "Bands (1-12)",
        NormalisableRange<float> (1.0f, 12.0f, 1.0f), 5.0f));

    layout.add (std::make_unique<AudioParameterInt> (
        "demonCount", "Demons (0-50)", 0, 50, 8));

    layout.add (std::make_unique<AudioParameterChoice> (
        "bitDepth", "Bit Depth",
        StringArray { "16", "20", "24" }, 2));

    layout.add (std::make_unique<AudioParameterChoice> (
        "genreId", "Genre",
        StringArray { "Neo-Classical", "Cinematic Pop", "Jazz/Hip-Hop", "Flat" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        "unisonWeight", "Unison Weight",
        NormalisableRange<float> (0.0f, 1.0f), 0.6f));

    layout.add (std::make_unique<AudioParameterFloat> (
        "outputGain", "Output",
        NormalisableRange<float> (-24.0f, 24.0f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        "mix", "Mix",
        NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    return layout;
}

bool MatchboxEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono()
        || input == juce::AudioChannelSet::stereo();
}

void MatchboxEQAudioProcessor::prepareToPlay (double sampleRate, int)
{
    eq = std::make_unique<MatchboxEQ> (static_cast<float> (sampleRate));
}

void MatchboxEQAudioProcessor::releaseResources()
{
    eq.reset();
}

void MatchboxEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int inputChannels = getTotalNumInputChannels();
    const int outputChannels = getTotalNumOutputChannels();

    for (int channel = inputChannels; channel < outputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    if (numSamples <= 0 || inputChannels <= 0 || buffer.getNumChannels() <= 0)
        return;

    float* left = buffer.getWritePointer (0);
    float* right = inputChannels > 1 && buffer.getNumChannels() > 1
                 ? buffer.getWritePointer (1) : nullptr;

    const float asked = apvts.getRawParameterValue ("askedBands")->load();
    const int demons = static_cast<int> (apvts.getRawParameterValue ("demonCount")->load());
    const int bitIndex = static_cast<int> (apvts.getRawParameterValue ("bitDepth")->load());
    const int genre = static_cast<int> (apvts.getRawParameterValue ("genreId")->load());
    const float unison = apvts.getRawParameterValue ("unisonWeight")->load();
    const float outputGain = juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue ("outputGain")->load());
    const float mix = apvts.getRawParameterValue ("mix")->load();
    const float bitDepth = bitIndex == 0 ? 16.0f : (bitIndex == 1 ? 20.0f : 24.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        // Sanitize the dry path as well as the DSP path. Otherwise mix=0
        // (or a partially dry mix) can pass NaN/Inf straight to the host even
        // though MatchboxEQ::process() sanitizes its own input copy.
        const float rawLeft = left[i];
        const float rawRight = right != nullptr ? right[i] : rawLeft;
        const float dryLeft = std::isfinite (rawLeft)
                            ? juce::jlimit (-64.0f, 64.0f, rawLeft) : 0.0f;
        const float dryRight = std::isfinite (rawRight)
                             ? juce::jlimit (-64.0f, 64.0f, rawRight) : 0.0f;
        float wetLeft = dryLeft;
        float wetRight = dryRight;

        if (eq != nullptr)
            eq->process (wetLeft, wetRight, asked, demons, bitDepth, genre, unison);

        left[i] = (dryLeft * (1.0f - mix) + wetLeft * mix) * outputGain;

        if (right != nullptr)
            right[i] = (dryRight * (1.0f - mix) + wetRight * mix) * outputGain;
    }
}

juce::AudioProcessorEditor* MatchboxEQAudioProcessor::createEditor()
{
    return new MatchboxEQAudioProcessorEditor (*this);
}

void MatchboxEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void MatchboxEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MatchboxEQAudioProcessor();
}
