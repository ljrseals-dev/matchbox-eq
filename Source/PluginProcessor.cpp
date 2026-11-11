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
        "demonCount", "Denons (1-50)", 1, 50, 8));

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

void MatchboxEQAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    eq = std::make_unique<MatchboxEQ> ((float) sampleRate);
}

void MatchboxEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    float* left  = buffer.getWritePointer (0);
    float* right = buffer.getWritePointer (1);

    const float asked   = apvts.getRawParameterValue ("askedBands")->load();
    const int   demons  = (int) apvts.getRawParameterValue ("demonCount")->load();
    const int   bitIdx  = (int) apvts.getRawParameterValue ("bitDepth")->load();
    const int   genre   = (int) apvts.getRawParameterValue ("genreId")->load();
    const float unison  = apvts.getRawParameterValue ("unisonWeight")->load();
    const float outGain = juce::Decibels::decibelsToGain (
                              apvts.getRawParameterValue ("outputGain")->load());
    const float mix     = apvts.getRawParameterValue ("mix")->load();

    const float bit = (bitIdx == 0) ? 16.0f : (bitIdx == 1) ? 20.0f : 24.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float dryL = left[i];
        float dryR = right[i];
        float wetL = left[i];
        float wetR = right[i];

        eq->process (wetL, wetR, asked, demons, bit, genre, unison);

        left[i]  = (dryL * (1.0f - mix) + wetL * mix) * outGain;
        right[i] = (dryR * (1.0f - mix) + wetR * mix) * outGain;
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
    copyXmlToBinary (*xml, destData);
}

void MatchboxEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MatchboxEQAudioProcessor();
}
