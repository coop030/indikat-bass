#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// Monophoner Bass-Synth: 2 Oszillatoren + Sub, Glide, Amp-ADSR und Filter-Hüllkurve.
// Die Filter-Hüllkurve wird nicht hier angewendet, sondern als Modulationssignal
// an den Env Filter der Effektkette übergeben.
class BassSynth
{
public:
    static const juce::StringArray waveNames;
    static const juce::StringArray chordNames;
    static constexpr int maxChordNotes = 5;

    static void addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& p);

    void init (juce::AudioProcessorValueTreeState& apvts);
    void prepare (double sampleRate);

    // Rendert in out (Mono, numSamples) und schreibt die Filter-Hüllkurve (0..1) nach filterMod
    void render (const juce::MidiBuffer& midi, float* out, float* filterMod, int numSamples);

    float getAmpEnvelope() const { return ampEnv; }

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void handleMidi (const juce::MidiMessage&);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void startNote (int note, bool retrigger);
    float oscillator (int wave, double phase, double inc) const;

    double sr = 44100.0;

    // Noten-Stack (letzte Note hat Priorität)
    juce::Array<int> heldNotes;
    int currentNote = -1;
    float targetPitch = 36.0f, pitch = 36.0f;
    float velocity = 1.0f;
    float pitchBend = 0.0f;

    // Oszillatoren (pro Akkordton eigene Phasen)
    double phase1[maxChordNotes] {}, phase2[maxChordNotes] {}, phaseSub = 0.0;
    float drift1 = 0.0f, drift2 = 0.0f, driftTarget1 = 0.0f, driftTarget2 = 0.0f;
    int driftCounter = 0;
    juce::Random random;

    // Hüllkurven
    Stage ampStage = Stage::Idle;
    float ampEnv = 0.0f;
    bool filterAttacking = false;
    float filterEnv = 0.0f;

    juce::SmoothedValue<float> level1, level2, levelSub;

    std::atomic<float>* pOsc1Wave; std::atomic<float>* pOsc1Oct; std::atomic<float>* pOsc1Level;
    std::atomic<float>* pOsc2Wave; std::atomic<float>* pOsc2Oct; std::atomic<float>* pOsc2Semi;
    std::atomic<float>* pOsc2Fine; std::atomic<float>* pOsc2Level; std::atomic<float>* pSubLevel;
    std::atomic<float>* pGlide; std::atomic<float>* pLegato; std::atomic<float>* pVelocity; std::atomic<float>* pDrift;
    std::atomic<float>* pAmpA; std::atomic<float>* pAmpD; std::atomic<float>* pAmpS; std::atomic<float>* pAmpR;
    std::atomic<float>* pFiltA; std::atomic<float>* pFiltD;
    std::atomic<float>* pChordOn; std::atomic<float>* pChordType;
};
