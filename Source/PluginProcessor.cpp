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
        // MIDI GATE: spielt nur, solange eine Note gehalten wird; ALWAYS ON: läuft durchgehend
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "trigger", 1 }, "Trigger",
                                                                  juce::StringArray { "MIDI GATE", "ALWAYS ON" }, 0));
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
    triggerParam = apvts.getRawParameterValue ("trigger");

    // Standalone hat meist kein MIDI: dort durchgehend spielen
    if (wrapperType == wrapperType_Standalone)
        if (auto* trigger = apvts.getParameter ("trigger"))
            trigger->setValueNotifyingHost (1.0f);
    randomize();
}

void PlutoProcessor::randomize()
{
    setSeed ((juce::uint32) juce::Random::getSystemRandom().nextInt());
}

void PlutoProcessor::setSeed (juce::uint32 s)
{
    seed.store (s);
    elements.store (PlutoEngine::sceneMask (s));
    customSelection.store (false);
    engine.requestScene (s);
}

void PlutoProcessor::toggleElement (int element)
{
    if (! juce::isPositiveAndBelow (element, (int) PlutoEngine::numElements))
        return;
    const auto mask = elements.load() ^ (1u << element);
    elements.store (mask);
    customSelection.store (true);
    engine.requestElements (mask);
}

bool PlutoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void PlutoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    heldNotes = {};
    numHeld = 0;
    engine.prepare (sampleRate, samplesPerBlock);
}

void PlutoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numOut = getTotalNumOutputChannels();
    buffer.clear();
    engine.setScale ((int) keyParam->load(), (int) scaleParam->load());

    // Gehaltene Noten zählen (Note-Off auch als Note-On mit Velocity 0, All Notes Off)
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn() && ! heldNotes[(size_t) msg.getNoteNumber()])
        {
            heldNotes[(size_t) msg.getNoteNumber()] = true;
            ++numHeld;
        }
        else if (msg.isNoteOff() && heldNotes[(size_t) msg.getNoteNumber()])
        {
            heldNotes[(size_t) msg.getNoteNumber()] = false;
            --numHeld;
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            heldNotes = {};
            numHeld = 0;
        }
    }
    engine.setGate (triggerParam->load() > 0.5f || numHeld > 0);

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
    if (customSelection.load())
        state.setProperty ("elements", juce::String::toHexString ((int) elements.load()), nullptr);
    else
        state.removeProperty ("elements", nullptr);
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
            if (xml->hasAttribute ("elements"))
            {
                const auto mask = (juce::uint32) xml->getStringAttribute ("elements").getHexValue32();
                elements.store (mask);
                customSelection.store (true);
                engine.requestElements (mask);
            }
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PlutoProcessor(); }
