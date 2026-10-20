#include "BassSynth.h"

const juce::StringArray BassSynth::waveNames { "Sine", "Triangle", "Saw", "Square", "Pulse" };
const juce::StringArray BassSynth::chordNames { "m7", "m9", "Minor", "Fifth" };

namespace
{
    // Akkord-Intervalle in Halbtönen (Grundton zuerst)
    struct Chord { int size; float intervals[BassSynth::maxChordNotes]; };
    const Chord chords[] = {
        { 4, { 0, 3, 7, 10, 0 } },      // m7: klassischer Dub-Chord
        { 5, { 0, 3, 7, 10, 14 } },     // m9
        { 3, { 0, 3, 7, 0, 0 } },       // Moll
        { 3, { 0, 7, 12, 0, 0 } },      // Quinte + Oktave
    };
}

namespace
{
    // PolyBLEP gegen Aliasing an den Sprungstellen von Saw/Square
    inline double polyBlep (double t, double dt)
    {
        if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    inline double wrap (double p) { return p - std::floor (p); }

    inline float coefFor (float ms, double sr) { return std::exp (-1.0f / (juce::jmax (0.05f, ms) * 0.001f * (float) sr)); }
}

void BassSynth::addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& p)
{
    using namespace juce;

    auto pct = AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
                   .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue() / 100.0f; });
    auto ms = AudioParameterFloatAttributes()
                  .withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s"
                                                                                          : String (v, v < 10.0f ? 1 : 0) + " ms"; })
                  .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });
    auto cents = AudioParameterFloatAttributes()
                     .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + String (roundToInt (v)) + " ct"; });
    auto signedInt = [] (int v, int) { return (v > 0 ? "+" : "") + String (v); };

    auto msRange = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "osc1Wave", 1 }, "Osc 1 Wave", waveNames, 2));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "osc1Oct", 1 }, "Osc 1 Octave", -2, 2, 0,
                                                      AudioParameterIntAttributes().withStringFromValueFunction (signedInt)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "osc1Level", 1 }, "Osc 1 Level", NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "osc2Wave", 1 }, "Osc 2 Wave", waveNames, 3));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "osc2Oct", 1 }, "Osc 2 Octave", -2, 2, 0,
                                                      AudioParameterIntAttributes().withStringFromValueFunction (signedInt)));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "osc2Semi", 1 }, "Osc 2 Semitones", -12, 12, 0,
                                                      AudioParameterIntAttributes().withStringFromValueFunction (signedInt)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "osc2Fine", 1 }, "Osc 2 Fine", NormalisableRange<float> (-50.0f, 50.0f, 1.0f), 0.0f, cents));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "osc2Level", 1 }, "Osc 2 Level", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "subLevel", 1 }, "Sub Osc", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "chordOn", 1 }, "Chord", StringArray { "Off", "On" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "chordType", 1 }, "Chord Type", chordNames, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "glide", 1 }, "Glide", msRange (0.0f, 1000.0f, 120.0f), 0.0f, ms));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "legato", 1 }, "Mode", StringArray { "Retrig", "Legato" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "velocity", 1 }, "Velocity", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "drift", 1 }, "Drift", NormalisableRange<float> (0.0f, 1.0f), 0.1f, pct));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ampAttack", 1 }, "Amp Attack", msRange (0.5f, 2000.0f, 50.0f), 1.0f, ms));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ampDecay", 1 }, "Amp Decay", msRange (5.0f, 3000.0f, 300.0f), 300.0f, ms));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ampSustain", 1 }, "Amp Sustain", NormalisableRange<float> (0.0f, 1.0f), 0.7f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ampRelease", 1 }, "Amp Release", msRange (5.0f, 3000.0f, 200.0f), 120.0f, ms));
}

void BassSynth::init (juce::AudioProcessorValueTreeState& apvts)
{
    auto get = [&apvts] (const char* id) { return apvts.getRawParameterValue (id); };
    pOsc1Wave = get ("osc1Wave");   pOsc1Oct = get ("osc1Oct");     pOsc1Level = get ("osc1Level");
    pOsc2Wave = get ("osc2Wave");   pOsc2Oct = get ("osc2Oct");     pOsc2Semi = get ("osc2Semi");
    pOsc2Fine = get ("osc2Fine");   pOsc2Level = get ("osc2Level"); pSubLevel = get ("subLevel");
    pGlide = get ("glide");         pLegato = get ("legato");       pVelocity = get ("velocity");
    pDrift = get ("drift");
    pAmpA = get ("ampAttack");      pAmpD = get ("ampDecay");       pAmpS = get ("ampSustain");
    pAmpR = get ("ampRelease");
    // Filter-Hüllkurve nutzt Attack/Release-Regler des Env Filters (im Synth: Attack/Decay)
    pFiltA = get ("envAttack");     pFiltD = get ("envRelease");
    pChordOn = get ("chordOn");     pChordType = get ("chordType");
}

void BassSynth::prepare (double sampleRate)
{
    sr = sampleRate;
    heldNotes.clearQuick();
    currentNote = -1;
    ampStage = Stage::Idle;
    ampEnv = filterEnv = 0.0f;
    std::fill (std::begin (phase1), std::end (phase1), 0.0);
    std::fill (std::begin (phase2), std::end (phase2), 0.0);
    phaseSub = 0.0;
    for (auto* s : { &level1, &level2, &levelSub })
        s->reset (sampleRate, 0.02);
    level1.setCurrentAndTargetValue (pOsc1Level->load());
    level2.setCurrentAndTargetValue (pOsc2Level->load());
    levelSub.setCurrentAndTargetValue (pSubLevel->load());
}

void BassSynth::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f;
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldNotes.clearQuick();
        if (ampStage != Stage::Idle)
            ampStage = Stage::Release;
    }
}

void BassSynth::noteOn (int note, float vel)
{
    const bool overlapping = ! heldNotes.isEmpty();
    heldNotes.removeFirstMatchingValue (note);
    heldNotes.add (note);

    const float sens = pVelocity->load();
    velocity = 1.0f - sens * (1.0f - vel);

    const bool legatoMode = pLegato->load() > 0.5f;
    startNote (note, ! (overlapping && legatoMode));
}

void BassSynth::noteOff (int note)
{
    heldNotes.removeFirstMatchingValue (note);

    if (note != currentNote)
        return;

    if (! heldNotes.isEmpty())
        startNote (heldNotes.getLast(), false); // zurück zur vorherigen gehaltenen Note, ohne Retrigger
    else if (ampStage != Stage::Idle)
        ampStage = Stage::Release;
}

void BassSynth::startNote (int note, bool retrigger)
{
    const bool fromSilence = ampStage == Stage::Idle || ampEnv < 0.001f;
    const bool glide = pGlide->load() > 0.0f && currentNote >= 0 && ! fromSilence;

    currentNote = note;
    targetPitch = (float) note;
    if (! glide)
        pitch = targetPitch;

    if (fromSilence)
    {
        // Phasen-Reset -> jede Note startet gleich (tight, kein Klick bei Sinus)
        std::fill (std::begin (phase1), std::end (phase1), 0.0);
        std::fill (std::begin (phase2), std::end (phase2), 0.0);
        phaseSub = 0.0;
    }

    if (retrigger || fromSilence)
    {
        ampStage = Stage::Attack;
        filterAttacking = true;
    }
}

float BassSynth::oscillator (int wave, double ph, double inc) const
{
    switch (wave)
    {
        case 0: return (float) std::sin (ph * juce::MathConstants<double>::twoPi);
        case 1: return (float) (ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph);
        case 2: return (float) (2.0 * ph - 1.0 - polyBlep (ph, inc));
        case 3:
        case 4:
        {
            const double width = wave == 3 ? 0.5 : 0.25;
            double v = ph < width ? 1.0 : -1.0;
            v += polyBlep (ph, inc);
            v -= polyBlep (wrap (ph + 1.0 - width), inc);
            return (float) (wave == 3 ? v : v + 0.5) * 0.9f; // Pulse DC-kompensiert
        }
        default: return 0.0f;
    }
}

void BassSynth::render (const juce::MidiBuffer& midi, float* out, float* filterMod, int numSamples)
{
    const int w1 = (int) pOsc1Wave->load(), w2 = (int) pOsc2Wave->load();
    const float oct1 = pOsc1Oct->load() * 12.0f;
    const float off2 = pOsc2Oct->load() * 12.0f + pOsc2Semi->load() + pOsc2Fine->load() * 0.01f;
    level1.setTargetValue (pOsc1Level->load());
    level2.setTargetValue (pOsc2Level->load());
    levelSub.setTargetValue (pSubLevel->load());

    const float glideMs = pGlide->load();
    const float glideCoef = glideMs > 0.0f ? coefFor (glideMs * 0.33f, sr) : 0.0f;
    const float driftCents = pDrift->load() * 12.0f;

    const float attackInc = 1.0f / (juce::jmax (0.5f, pAmpA->load()) * 0.001f * (float) sr);
    const float decayCoef = coefFor (pAmpD->load() * 0.3f, sr);
    const float sustain = pAmpS->load();
    const float releaseCoef = coefFor (pAmpR->load() * 0.3f, sr);
    const float fAttackInc = 1.0f / (juce::jmax (0.1f, pFiltA->load()) * 0.001f * (float) sr);
    const float fDecayCoef = coefFor (pFiltD->load() * 0.4f, sr);

    const auto& chord = chords[juce::jlimit (0, (int) std::size (chords) - 1, (int) pChordType->load())];
    const int numNotes = pChordOn->load() > 0.5f ? chord.size : 1;
    const float chordGain = 1.0f / std::sqrt ((float) numNotes);

    auto midiIt = midi.begin();

    for (int n = 0; n < numSamples; ++n)
    {
        while (midiIt != midi.end() && (*midiIt).samplePosition <= n)
        {
            handleMidi ((*midiIt).getMessage());
            ++midiIt;
        }

        // Analoger Drift: langsamer Zufallsweg pro Oszillator
        if (--driftCounter <= 0)
        {
            driftCounter = (int) (sr * 0.05);
            driftTarget1 = (random.nextFloat() * 2.0f - 1.0f) * driftCents;
            driftTarget2 = (random.nextFloat() * 2.0f - 1.0f) * driftCents;
        }
        drift1 += 0.0002f * (driftTarget1 - drift1);
        drift2 += 0.0002f * (driftTarget2 - drift2);

        pitch = targetPitch + glideCoef * (pitch - targetPitch);
        const float base = pitch + pitchBend;

        // Amp-Hüllkurve
        switch (ampStage)
        {
            case Stage::Attack:
                ampEnv += attackInc;
                if (ampEnv >= 1.0f) { ampEnv = 1.0f; ampStage = Stage::Decay; }
                break;
            case Stage::Decay:
                ampEnv = sustain + decayCoef * (ampEnv - sustain);
                if (ampEnv - sustain < 0.0005f) ampStage = Stage::Sustain;
                break;
            case Stage::Sustain:
                ampEnv = sustain;
                if (sustain <= 0.0f) ampStage = Stage::Idle;
                break;
            case Stage::Release:
                ampEnv *= releaseCoef;
                if (ampEnv < 0.00005f) { ampEnv = 0.0f; ampStage = Stage::Idle; }
                break;
            case Stage::Idle:
                ampEnv = 0.0f;
                break;
        }

        // Filter-Hüllkurve (Attack/Decay, wird bei jedem Retrigger neu gestartet)
        if (filterAttacking)
        {
            filterEnv += fAttackInc;
            if (filterEnv >= 1.0f) { filterEnv = 1.0f; filterAttacking = false; }
        }
        else
            filterEnv *= fDecayCoef;

        filterMod[n] = filterEnv * velocity;

        if (ampStage == Stage::Idle)
        {
            out[n] = 0.0f;
            level1.skip (1); level2.skip (1); levelSub.skip (1);
            continue;
        }

        const float l1 = level1.getNextValue(), l2 = level2.getNextValue();
        float s = 0.0f;

        // Jeder Akkordton bekommt OSC 1 + OSC 2; Glide zieht den ganzen Akkord mit
        for (int k = 0; k < numNotes; ++k)
        {
            const float note = base + chord.intervals[k] * (numNotes > 1 ? 1.0f : 0.0f);
            const double f1 = 440.0 * std::exp2 ((note + oct1 + drift1 * 0.01f - 69.0f) / 12.0f);
            const double f2 = 440.0 * std::exp2 ((note + off2 + drift2 * 0.01f - 69.0f) / 12.0f);
            const double i1 = juce::jmin (0.45, f1 / sr), i2 = juce::jmin (0.45, f2 / sr);
            s += chordGain * (l1 * oscillator (w1, phase1[k], i1) + l2 * oscillator (w2, phase2[k], i2));
            phase1[k] = wrap (phase1[k] + i1);
            phase2[k] = wrap (phase2[k] + i2);
        }

        // Sub nur auf dem Grundton
        const double is = 440.0 * std::exp2 ((base + oct1 - 12.0f - 69.0f) / 12.0f) / sr;
        s += levelSub.getNextValue() * (float) std::sin (phaseSub * juce::MathConstants<double>::twoPi);
        phaseSub = wrap (phaseSub + is);

        out[n] = s * ampEnv * velocity * 0.5f;
    }
}
