#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    struct Preset
    {
        const char* name;
        const char* description;
        std::initializer_list<std::pair<const char*, float>> values;
    };

    // Nur Abweichungen von den Defaults (siehe createLayout).
    // Wellenform: 0 Saw, 1 Square, 2 Triangle, 3 Sine · Set: 0 Deep, 1 Dub, 2 Fifths · Voicing: 0 Wide, 1 Close
    // Puls: 0 Pad, 1 Offbeat, 2 Dub, 3 Syncopated, 4 Eighths · Echo: 0 1/8, 1 1/8 D, 2 1/4, 3 1/4 D
    // LFO: 0 1 Bar … 5 32 Bars
    const Preset presets[] = {
        // Deep
        { "Andromeda", "Warm saw pad with a slowly breathing filter.",
          { { "detune", 16 }, { "cutoff", 1200 }, { "reso", 0.47f }, { "attack", 1.8f }, { "release", 3.5f },
            { "chorus", 0.6f }, { "revMix", 0.45f }, { "revSize", 0.72f }, { "stars", 0.25f }, { "noise", 0.08f } } },
        { "Vega", "Soft triangles, bright and airy.",
          { { "wave", 2 }, { "voices", 4 }, { "detune", 10 }, { "cutoff", 2600 }, { "reso", 0.21f }, { "attack", 2.5f },
            { "release", 4 }, { "revMix", 0.55f }, { "revSize", 0.77f }, { "stars", 0.35f }, { "sub", 0.3f } } },
        { "Milky Way", "A wide wall of seven voices.",
          { { "voices", 7 }, { "detune", 24 }, { "cutoff", 900 }, { "reso", 0.29f }, { "lfoRate", 3 }, { "lfoDepth", 0.5f },
            { "attack", 3 }, { "release", 5 }, { "chorus", 0.7f }, { "revMix", 0.6f }, { "revSize", 0.85f },
            { "stars", 0.5f }, { "starHeight", 3 } } },
        { "Nebula", "Dark square wave inside the noise.",
          { { "wave", 1 }, { "voices", 3 }, { "detune", 8 }, { "cutoff", 700 }, { "reso", 0.55f }, { "lfoRate", 2 },
            { "lfoDepth", 0.4f }, { "noise", 0.3f }, { "attack", 2 }, { "release", 3 }, { "revMix", 0.5f } } },
        { "Lyra", "Close voicing with a round sub below.",
          { { "wave", 2 }, { "voices", 3 }, { "detune", 6 }, { "sub", 0.45f }, { "cutoff", 3000 }, { "voicing", 1 },
            { "attack", 1 }, { "release", 2 }, { "stars", 0.2f } } },
        { "Deneb", "Low and resonant, very slow movement.",
          { { "detune", 20 }, { "octave", -1 }, { "cutoff", 600 }, { "reso", 0.69f }, { "lfoRate", 4 }, { "lfoDepth", 0.7f },
            { "attack", 4 }, { "release", 6 }, { "revMix", 0.5f }, { "revSize", 0.81f }, { "sub", 0 } } },
        // Dub
        { "Orion Dub", "Classic dub stab with a dotted echo.",
          { { "voices", 3 }, { "detune", 8 }, { "attack", 0.005f }, { "release", 0.35f }, { "cutoff", 1100 }, { "reso", 0.63f },
            { "chordSet", 1 }, { "voicing", 1 }, { "pulse", 2 }, { "gate", 0.12f }, { "feedback", 0.65f }, { "delayMix", 0.45f },
            { "revMix", 0.35f }, { "revSize", 0.58f }, { "chorus", 0.3f }, { "stars", 0.15f }, { "noise", 0.15f }, { "sub", 0 } } },
        { "Sirius Echo", "Offbeat stabs bouncing ping-pong.",
          { { "wave", 1 }, { "voices", 2 }, { "detune", 6 }, { "attack", 0.005f }, { "release", 0.3f }, { "cutoff", 1600 },
            { "reso", 0.47f }, { "chordSet", 1 }, { "voicing", 1 }, { "pulse", 1 }, { "gate", 0.1f }, { "feedback", 0.7f },
            { "delayMix", 0.5f }, { "revMix", 0.3f }, { "stars", 0.12f }, { "sub", 0 } } },
        { "Comet Tail", "Long echo trail, wandering filter.",
          { { "voices", 4 }, { "detune", 12 }, { "attack", 0.01f }, { "release", 0.6f }, { "cutoff", 900 }, { "reso", 0.75f },
            { "lfoRate", 3 }, { "lfoDepth", 0.5f }, { "chordSet", 1 }, { "pulse", 3 }, { "gate", 0.14f }, { "delayDiv", 3 },
            { "feedback", 0.78f }, { "delayMix", 0.55f }, { "revMix", 0.45f }, { "stars", 0.2f }, { "sub", 0 } } },
        { "Dark Matter", "Deep stab, lots of vinyl.",
          { { "voices", 3 }, { "detune", 10 }, { "octave", -1 }, { "cutoff", 500 }, { "reso", 0.81f }, { "attack", 0.01f },
            { "release", 0.5f }, { "chordSet", 1 }, { "voicing", 1 }, { "pulse", 2 }, { "gate", 0.15f }, { "feedback", 0.72f },
            { "delayMix", 0.5f }, { "noise", 0.35f }, { "revMix", 0.4f }, { "stars", 0.1f } } },
        { "Pulsar", "Short eighths, fast filter pulse.",
          { { "wave", 1 }, { "voices", 3 }, { "detune", 9 }, { "attack", 0.003f }, { "release", 0.2f }, { "gate", 0.06f },
            { "pulse", 4 }, { "cutoff", 1300 }, { "reso", 0.69f }, { "lfoRate", 1 }, { "lfoDepth", 0.45f }, { "chordSet", 1 },
            { "feedback", 0.5f }, { "delayMix", 0.35f }, { "revMix", 0.3f }, { "stars", 0.18f }, { "sub", 0 } } },
        { "Echo Chamber", "Plucked pad, feedback at the edge.",
          { { "wave", 2 }, { "voices", 3 }, { "detune", 7 }, { "attack", 0.02f }, { "release", 1.2f }, { "cutoff", 2000 },
            { "reso", 0.47f }, { "chordSet", 1 }, { "feedback", 0.82f }, { "delayMix", 0.6f }, { "revMix", 0.5f },
            { "revSize", 0.77f }, { "stars", 0.25f }, { "sub", 0.1f } } },
        // Minimal
        { "Polaris", "Open fifths, almost pure tone.",
          { { "wave", 2 }, { "voices", 2 }, { "detune", 4 }, { "chordSet", 2 }, { "voicing", 1 }, { "cutoff", 2000 },
            { "reso", 0.21f }, { "attack", 1 }, { "release", 2 }, { "revMix", 0.35f }, { "stars", 0.2f }, { "chorus", 0.3f },
            { "sub", 0.3f } } },
        { "Rigel", "Resonant fifth dabs on the offbeat.",
          { { "voices", 2 }, { "detune", 3 }, { "chordSet", 2 }, { "cutoff", 800 }, { "reso", 0.86f }, { "lfoRate", 2 },
            { "lfoDepth", 0.6f }, { "pulse", 1 }, { "gate", 0.08f }, { "release", 0.15f }, { "attack", 0.002f },
            { "feedback", 0.4f }, { "delayMix", 0.3f }, { "revMix", 0.3f }, { "stars", 0.1f }, { "sub", 0 } } },
        { "Mira", "One oscillator, high resonance, lots of time.",
          { { "wave", 1 }, { "voices", 1 }, { "detune", 0 }, { "chordSet", 2 }, { "cutoff", 600 }, { "reso", 0.91f },
            { "lfoRate", 4 }, { "lfoDepth", 0.8f }, { "attack", 2 }, { "release", 3 }, { "chorus", 0.2f }, { "revMix", 0.3f },
            { "stars", 0.15f }, { "sub", 0 } } },
        { "Quasar", "Dry eighth-note stabs, click-short.",
          { { "voices", 3 }, { "detune", 6 }, { "chordSet", 1 }, { "voicing", 1 }, { "pulse", 4 }, { "gate", 0.05f },
            { "release", 0.12f }, { "attack", 0.002f }, { "cutoff", 2200 }, { "reso", 0.55f }, { "feedback", 0.3f },
            { "delayMix", 0.2f }, { "revMix", 0.2f }, { "noise", 0.05f }, { "stars", 0.1f }, { "chorus", 0.2f }, { "sub", 0 } } },
        { "Zenith", "Fifths with sub, clean and bright.",
          { { "wave", 2 }, { "voices", 3 }, { "detune", 8 }, { "chordSet", 2 }, { "sub", 0.5f }, { "attack", 0.5f },
            { "release", 1.5f }, { "cutoff", 1800 }, { "revMix", 0.25f }, { "delayMix", 0.2f }, { "feedback", 0.4f },
            { "stars", 0.2f } } },
        { "Nadir", "Low fifths under a layer of noise.",
          { { "voices", 4 }, { "detune", 12 }, { "octave", -1 }, { "chordSet", 2 }, { "cutoff", 400 }, { "reso", 0.63f },
            { "attack", 3 }, { "release", 4 }, { "noise", 0.35f }, { "revMix", 0.5f }, { "lfoRate", 5 }, { "lfoDepth", 0.6f },
            { "stars", 0.15f }, { "sub", 0 } } },
        // Stars
        { "Pleiades", "Bright pad, dense sparkle on top.",
          { { "wave", 2 }, { "detune", 12 }, { "cutoff", 3500 }, { "attack", 2 }, { "release", 4 }, { "stars", 0.7f },
            { "starHeight", 3 }, { "starLevel", 0.6f }, { "revMix", 0.6f }, { "revSize", 0.81f }, { "delayMix", 0.35f },
            { "feedback", 0.55f } } },
        { "Shooting Star", "Sparks running through an eighth echo.",
          { { "detune", 18 }, { "cutoff", 1600 }, { "stars", 0.55f }, { "starLevel", 0.7f }, { "delayDiv", 0 },
            { "feedback", 0.6f }, { "delayMix", 0.4f }, { "revMix", 0.5f } } },
        { "Stardust", "Sine bed with bells everywhere.",
          { { "wave", 3 }, { "voices", 3 }, { "detune", 8 }, { "sub", 0.3f }, { "cutoff", 5000 }, { "reso", 0.21f },
            { "stars", 0.9f }, { "starHeight", 3 }, { "starLevel", 0.45f }, { "revMix", 0.7f }, { "revSize", 0.85f },
            { "delayMix", 0.3f }, { "feedback", 0.5f }, { "attack", 3 }, { "release", 5 } } },
        { "Cassiopeia", "Muted square, medium sparkle.",
          { { "wave", 1 }, { "voices", 4 }, { "detune", 10 }, { "cutoff", 1100 }, { "reso", 0.47f }, { "stars", 0.5f },
            { "revMix", 0.5f }, { "starLevel", 0.55f } } },
        { "Night Sky", "Huge and slow, distant stars.",
          { { "voices", 7 }, { "detune", 28 }, { "cutoff", 800 }, { "lfoRate", 4 }, { "lfoDepth", 0.5f }, { "attack", 4 },
            { "release", 7 }, { "stars", 0.4f }, { "starHeight", 3 }, { "noise", 0.12f }, { "revMix", 0.65f },
            { "revSize", 0.89f }, { "chorus", 0.6f } } },
        { "Supernova", "Everything open, maximum shine.",
          { { "voices", 7 }, { "detune", 35 }, { "cutoff", 2800 }, { "lfoRate", 2 }, { "lfoDepth", 0.35f }, { "stars", 1 },
            { "starLevel", 0.8f }, { "delayMix", 0.45f }, { "feedback", 0.65f }, { "revMix", 0.6f }, { "chorus", 0.8f } } },
    };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout StarPadProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dB  = AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; })
                   .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });
    auto hz  = AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz"
                                                                                         : String (roundToInt (v)) + " Hz"; })
                   .withValueFromStringFunction ([] (const String& t) { auto v = t.getFloatValue();
                                                                         return t.containsIgnoreCase ("k") ? v * 1000.0f : v; });
    auto sec = AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([] (float v, int) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms"
                                                                                     : String (v, 1) + " s"; })
                   .withValueFromStringFunction ([] (const String& t) { auto v = t.getFloatValue();
                                                                         return t.containsIgnoreCase ("ms") ? v / 1000.0f : v; });
    auto pct = AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
                   .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue() / 100.0f; });
    auto cents = AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ct"; })
                   .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });

    auto choice = [&] (const char* id, const char* name, const StringArray& items, int def)
    { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto real = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, const AudioParameterFloatAttributes& a)
    { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a)); };
    auto integer = [&] (const char* id, const char* name, int lo, int hi, int def)
    { p.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };

    // Klang
    choice ("wave", "Wave", StarPadEngine::waveNames, 0);
    integer ("voices", "Voices", 1, 7, 5);
    real ("detune", "Detune", NormalisableRange<float> (0.0f, 50.0f, 0.1f), 14.0f, cents);
    integer ("octave", "Octave", -1, 1, 0);
    real ("sub", "Sub", NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct);

    // Akkorde
    choice ("chordSet", "Chord Set", StarPadEngine::chordSetNames, 0);
    choice ("voicing", "Voicing", StarPadEngine::voicingNames, 0);
    choice ("chordOn", "MIDI Chord", StringArray { "Off", "On" }, 0);
    choice ("key", "Pad Key", StarPadEngine::keyNames, 9);

    // Hüllkurve & Puls
    real ("attack", "Attack", skewed (0.002f, 6.0f, 0.5f), 1.2f, sec);
    real ("release", "Release", skewed (0.05f, 8.0f, 1.0f), 2.5f, sec);
    choice ("pulse", "Pulse", StarPadEngine::pulseNames, 0);
    real ("gate", "Stab Length", skewed (0.03f, 1.0f, 0.15f), 0.25f, sec);

    // Filter
    real ("cutoff", "Cutoff", skewed (100.0f, 8000.0f, 1000.0f), 1400.0f, hz);
    real ("reso", "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.36f, pct);
    choice ("lfoRate", "LFO Rate", StarPadEngine::lfoRateNames, 3);
    real ("lfoDepth", "LFO Depth", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);

    // Raum
    real ("chorus", "Chorus", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
    choice ("delayDiv", "Echo Time", StarPadEngine::delayDivNames, 1);
    real ("feedback", "Feedback", NormalisableRange<float> (0.0f, 0.9f), 0.45f, pct);
    real ("delayMix", "Echo Mix", NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct);
    real ("revSize", "Reverb Size", NormalisableRange<float> (0.0f, 1.0f), 0.66f, pct);
    real ("revMix", "Reverb Mix", NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct);

    // Sterne
    real ("stars", "Star Density", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);
    integer ("starHeight", "Star Height", 1, 3, 2);
    real ("starLevel", "Star Level", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
    real ("noise", "Vinyl", NormalisableRange<float> (0.0f, 1.0f), 0.1f, pct);

    real ("outGain", "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, dB);

    return { p.begin(), p.end() };
}

//==============================================================================
StarPadProcessor::StarPadProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "StarPad", createLayout())
{
    setCurrentProgram (0);
}

StarPadParams StarPadProcessor::readParams() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto i = [&f] (const char* id) { return juce::roundToInt (f (id)); };

    StarPadParams p;
    p.wave = i ("wave");          p.voices = i ("voices");      p.detune = f ("detune");
    p.octave = i ("octave");      p.sub = f ("sub");
    p.chordSet = i ("chordSet");  p.voicing = i ("voicing");    p.chordOn = i ("chordOn") != 0;
    p.key = i ("key");
    p.attack = f ("attack");      p.release = f ("release");    p.pulse = i ("pulse");
    p.gate = f ("gate");
    p.cutoff = f ("cutoff");      p.reso = f ("reso");          p.lfoRate = i ("lfoRate");
    p.lfoDepth = f ("lfoDepth");
    p.chorus = f ("chorus");      p.delayDiv = i ("delayDiv");  p.feedback = f ("feedback");
    p.delayMix = f ("delayMix");  p.revSize = f ("revSize");    p.revMix = f ("revMix");
    p.stars = f ("stars");        p.starHeight = i ("starHeight");
    p.starLevel = f ("starLevel"); p.noise = f ("noise");
    p.outGain = f ("outGain");
    return p;
}

bool StarPadProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void StarPadProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.setParams (readParams());
    engine.prepare (sampleRate);
    engine.setParams (readParams());
    keyboardState.reset();
}

void StarPadProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // Tempo und Position vom Host; ohne laufenden Transport läuft das Raster frei weiter
    double bpm = 122.0, ppq = freeRunPpq;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
            if (playing)
                if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    hostBpm.store ((float) bpm);

    engine.setParams (readParams());
    engine.setPad (padRequest.load());

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    float monoScratch[512];

    const double ppqPerSample = bpm / 60.0 / sr;
    int pos = 0;
    auto render = [&] (int end)
    {
        while (pos < end)
        {
            const int n = juce::jmin (end - pos, 512);
            engine.process (L + pos, R != nullptr ? R + pos : monoScratch, n, bpm, ppq + pos * ppqPerSample);
            if (R == nullptr)
                for (int k = 0; k < n; ++k) L[pos + k] = 0.5f * (L[pos + k] + monoScratch[k]);
            pos += n;
        }
    };

    for (const auto meta : midi)
    {
        render (juce::jlimit (0, numSamples, meta.samplePosition));
        const auto m = meta.getMessage();
        if (m.isNoteOn())                         engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())                   engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    render (numSamples);

    freeRunPpq = std::fmod ((playing ? ppq : freeRunPpq) + numSamples * ppqPerSample, 4096.0);
}

//==============================================================================
int StarPadProcessor::getNumPrograms() { return (int) std::size (presets); }

const juce::String StarPadProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? presets[index].name : "";
}

const juce::String StarPadProcessor::getProgramDescription (int index) const
{
    return juce::isPositiveAndBelow (index, (int) std::size (presets)) ? presets[index].description : "";
}

void StarPadProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;

    // Erst alles auf Default (Tonart bleibt), dann die Preset-Werte
    for (auto* param : getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (p->getParameterID() != "key")
                p->setValueNotifyingHost (p->getDefaultValue());

    for (auto& [id, value] : presets[index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void StarPadProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void StarPadProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* StarPadProcessor::createEditor() { return new StarPadEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StarPadProcessor(); }
