#include "PluginProcessor.h"
#include "PluginEditor.h"

PlutoProcessor::PlutoProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
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
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == out;
}

void PlutoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

void PlutoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

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
    juce::XmlElement xml ("PLUTO");
    xml.setAttribute ("seed", juce::String::toHexString ((int) seed.load()));
    copyXmlToBinary (xml, dest);
}

void PlutoProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName ("PLUTO") && xml->hasAttribute ("seed"))
            setSeed ((juce::uint32) xml->getStringAttribute ("seed").getHexValue32());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PlutoProcessor(); }
