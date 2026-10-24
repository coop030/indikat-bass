#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::StringArray keys, scales;
        for (int i = 0; i < 12; ++i) keys.add (PlutoEngine::noteName (i));
        for (int i = 0; i < PlutoEngine::numScales; ++i) scales.add (PlutoEngine::scaleName (i));

        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "key", 1 }, "Key", keys, 9));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "scale", 1 }, "Scale", scales, 4));
        return layout;
    }
}

// Generator: kein Audio-Eingang, die Atmo entsteht komplett im Plugin
PlutoProcessor::PlutoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PLUTO", createLayout())
{
    keyParam = apvts.getRawParameterValue ("key");
    scaleParam = apvts.getRawParameterValue ("scale");
    randomize();
}

void PlutoProcessor::randomize()
{
    setSeed ((juce::uint32) juce::Random::getSystemRandom().nextInt());
}

void PlutoProcessor::setSeed (juce::uint32 s)
{
    seed.store (s);
    engine.requestScene (s);
}

bool PlutoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void PlutoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

void PlutoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numOut = getTotalNumOutputChannels();
    buffer.clear();
    engine.setScale ((int) keyParam->load(), (int) scaleParam->load());

    double bpm = 122.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) { ppq = *p; playing = pos->getIsPlaying(); }
        }
    bpmShown.store ((float) bpm);

    if (numOut >= 2)
    {
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples(), bpm, ppq, playing);
    }
    else if (numOut == 1)
    {
        // Mono: Atmo auf einen Hilfspuffer rendern und summieren
        juce::HeapBlock<float> l ((size_t) buffer.getNumSamples(), true), r ((size_t) buffer.getNumSamples(), true);
        engine.process (l, r, buffer.getNumSamples(), bpm, ppq, playing);
        auto* o = buffer.getWritePointer (0);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            o[i] += 0.5f * (l[i] + r[i]);
    }
}

juce::AudioProcessorEditor* PlutoProcessor::createEditor() { return new PlutoEditor (*this); }

void PlutoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("seed", juce::String::toHexString ((int) seed.load()), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PlutoProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName ("PLUTO"))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            if (xml->hasAttribute ("seed"))
                setSeed ((juce::uint32) xml->getStringAttribute ("seed").getHexValue32());
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PlutoProcessor(); }
