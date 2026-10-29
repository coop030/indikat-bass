#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DubReverb.h"

// Alle Klangparameter in einem Block, so wie sie der Prozessor pro Audio-Block übergibt
struct StarPadParams
{
    int wave = 0;            // 0 Saw, 1 Square, 2 Triangle, 3 Sine
    int voices = 5;          // Unison-Stimmen pro Ton
    float detune = 14.0f;    // Cent (Außenstimmen)
    int octave = 0;
    float sub = 0.2f;

    int chordSet = 0;        // 0 Deep, 1 Dub, 2 Fifths
    int voicing = 0;         // 0 Wide, 1 Close
    bool chordOn = false;    // MIDI-Note spielt einen ganzen Akkord
    int key = 9;             // Tonart der Pads (Moll), 9 = A

    float attack = 1.2f;     // s
    float release = 2.5f;    // s
    int pulse = 0;           // 0 Off, 1 Offbeat, 2 Dub, 3 Syncopated, 4 Eighths
    float gate = 0.25f;      // s, Länge eines Stabs

    float cutoff = 1400.0f;
    float reso = 0.36f;
    int lfoRate = 2;
    float lfoDepth = 0.3f;

    float chorus = 0.5f;
    int delayDiv = 1;
    float feedback = 0.45f;
    float delayMix = 0.25f;
    float revSize = 0.66f;
    float revMix = 0.4f;

    float stars = 0.3f;
    int starHeight = 2;
    float starLevel = 0.5f;
    float noise = 0.1f;

    float outGain = 0.0f;    // dB
};

// Polyphoner Pad-Synth für Minimal/Dub House: Unison-Oszillatoren, Akkord-Pads, tempo-synchrone
// Dub-Stabs, 24-dB-Filter mit LFO, Chorus, Ping-Pong-Echo, FDN-Hall, FM-Glöckchen ("Sterne") und Vinyl.
class StarPadEngine
{
public:
    static constexpr int numPads = 8;
    static const juce::StringArray waveNames, chordSetNames, voicingNames, pulseNames, lfoRateNames, delayDivNames, keyNames;

    static juce::String padLabel (int chordSet, int key, int pad);
    static juce::String padDegree (int chordSet, int pad);
    static int padChord (const StarPadParams&, int pad, int* notes, int& root); // liefert Anzahl Töne (max. 8)

    void prepare (double sampleRate);
    void setParams (const StarPadParams&);

    void noteOn (int note, float velocity);
    void noteOff (int note);
    void setPad (int pad);          // -1 = kein Pad gehalten
    void allNotesOff();

    // Rendert n Samples. ppq = Position am Blockanfang in Vierteln
    void process (float* left, float* right, int n, double bpm, double ppq);

    // Anzeige (GUI liest, Audio schreibt)
    struct Spark { float pitch, level; bool violet; };
    juce::AbstractFifo sparkFifo { 64 };
    Spark sparkBuffer[64] {};
    std::atomic<juce::uint64> heldMask[2] { {0}, {0} };
    std::atomic<float> level { 0.0f };
    static constexpr int scopeSize = 2048;
    float scope[scopeSize] {};
    std::atomic<int> scopePos { 0 };

private:
    static constexpr int maxVoices = 72, maxOsc = 7, maxSparks = 24;
    static constexpr int padTag = 200, stabTag = -1;

    struct Voice
    {
        bool active = false, isSub = false;
        int tag = 0, nOsc = 1, wave = 0;
        float freq = 0.0f;
        float phase[maxOsc] {}, spread[maxOsc] {}, jitter[maxOsc] {}, inc[maxOsc] {}, gainL[maxOsc] {}, gainR[maxOsc] {};
        float env = 0.0f, peak = 0.0f, attackStep = 0.0f, relCoef = 0.999f;
        int stage = 0;                // 0 Attack, 1 Halten, 2 Release
        int gateLeft = -1;            // Samples bis zum automatischen Release (Stab), -1 = gehalten
        juce::uint32 age = 0;
    };

    struct SparkVoice
    {
        bool active = false;
        float car = 0.0f, mod = 0.0f, f = 0.0f, index = 0.0f, indexCoef = 0.0f;
        float amp = 0.0f, peak = 0.0f, attackStep = 0.0f, decayCoef = 0.0f;
        float gL = 0.7f, gR = 0.7f;
        bool attacking = true;
    };

    struct Source { int tag = 0; int notes[8] {}; int count = 0; int root = 0; float peak = 0.0f, subScale = 1.0f; };

    void buildSource (int tag, int midiNote, float velocity, Source&) const;
    int collectSources (Source* out) const;   // max. 129 Quellen (128 MIDI + Pad)
    void startSource (const Source&, int gateSamples);
    void releaseTag (int tag);
    void releaseAllHeld();
    Voice& allocVoice();
    void startVoice (int note, float peak, int tag, int gateSamples, bool sub);
    void spawnSpark();
    void updateHeldMask();
    void refreshIncrements (Voice&) const;
    float readLine (const std::vector<float>& buf, int writePos, float delay) const;

    double sr = 44100.0;
    StarPadParams p;
    bool prepared = false;

    Voice voices[maxVoices];
    SparkVoice sparks[maxSparks];
    juce::uint32 ageCounter = 0;

    bool midiHeld[128] {};
    float midiVel[128] {};
    int currentPad = -1;
    juce::int64 lastStep = std::numeric_limits<juce::int64>::min();

    juce::Random rng;
    Source sourceScratch[129];
    int heldNotes[128] {};
    int heldCount = 0;

    juce::dsp::StateVariableTPTFilter<float> filt1, filt2;
    float lfoValue = 0.0f;

    // Chorus
    std::vector<float> chorusBuf[2];
    int chorusPos = 0;
    float chorusPhase[2] { 0.0f, 1.3f };

    // Ping-Pong-Echo mit Filtern in der Rückkopplung
    std::vector<float> delayBuf[2];
    int delayPos = 0;
    juce::SmoothedValue<float> delaySamples;
    juce::dsp::StateVariableTPTFilter<float> fbLow, fbHigh;
    float wobblePhase = 0.0f;

    DubReverb reverb;

    // Sterne-Bus
    juce::dsp::StateVariableTPTFilter<float> sparkHP;

    // Vinyl
    float noiseState[2] {};
    juce::dsp::StateVariableTPTFilter<float> noiseBP;

    juce::SmoothedValue<float> chorusS, fbS, delayMixS, revMixS, noiseS, outS;
};
