#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

// Generative Atmosphäre aus zufällig gelayerten Elementen.
// Jede Szene entsteht aus einem Seed: welche Elemente, wie dicht, welches Delay, welcher Raum.
class PlutoEngine
{
public:
    // Flächen (laufen durchgehend) und Ereignisse (tauchen im 16tel-Raster auf)
    enum Element
    {
        Vinyl, Hiss, SolarWind, Rain, Rumble, Shortwave,     // Flächen
        Blips, Drips, Pings, Static, Glitch, Sweeps,         // Ereignisse
        numElements
    };
    static constexpr int numBeds = 6;
    static constexpr int numEventTypes = numElements - numBeds;
    static const char* elementName (int);

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // Neue Szene anfordern (thread-sicher, wird am nächsten Block übernommen)
    void requestScene (juce::uint32 seed) { pendingSeed.store ((juce::int64) seed); }

    // Addiert die Atmo auf left/right (Dry bleibt unangetastet)
    void process (float* left, float* right, int numSamples, double bpm, double ppqAtStart, bool hostPlaying);

    // Für die Anzeige (Audio-Thread schreibt, UI liest)
    struct Display
    {
        std::atomic<juce::uint32> seed { 0 }, activeMask { 0 };
        std::array<std::atomic<float>, numElements> activity {};

        static constexpr int ringSize = 32;
        std::atomic<int> eventCount { 0 };
        std::array<std::atomic<int>, ringSize> eventType {};
        std::array<std::atomic<float>, ringSize> eventPan {};
    } display;

private:
    struct OnePole
    {
        float a = 0.0f, z = 0.0f;
        void setLowpass (float freq, double sr) { a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * freq / (float) sr); }
        float lp (float x) { z += a * (x - z); return z; }
        float hp (float x) { return x - lp (x); }
    };

    struct Scene
    {
        std::array<float, numBeds> bedGain {};
        std::array<float, numEventTypes> eventWeight {};
        float eventDensity = 0.0f;          // Wahrscheinlichkeit pro 16tel
        float crackleRate = 20.0f, rainRate = 40.0f, windCentre = 1.0f, rumbleFreq = 110.0f, shortwaveBase = 900.0f;
        float blipRegister = 1.0f;          // Oktavlage der Blips
        int delayStepsL = 3, delayStepsR = 4;
        float feedback = 0.5f, verbSend = 0.5f, roomSize = 0.9f, tone = 9000.0f, width = 1.0f;
    };

    struct Voice
    {
        bool active = false;
        int type = 0;
        int age = 0, length = 0;
        float pan = 0.5f, gain = 0.0f, sendDelay = 0.0f, sendVerb = 0.0f;
        double phase = 0.0;
        float freq = 1000.0f, freqEnd = 1000.0f;
        int repeatLen = 1;
        float holdValue = 0.0f;
        int holdCounter = 0, holdPeriod = 1;
        float bitSteps = 16.0f, chop = 30.0f;
        juce::dsp::StateVariableTPTFilter<float> bp;
    };

    struct Drop { float phase = 0.0f, inc = 0.0f, env = 0.0f, decay = 0.0f, pan = 0.5f; };

    static Scene makeScene (juce::uint32 seed);
    float noise() { return rng.nextFloat() * 2.0f - 1.0f; }
    float pink (int ch);
    void triggerEvent (double samplesPerStep);
    float renderVoice (Voice&);
    void publishEvent (int type, float pan);

    double sr = 44100.0;
    juce::Random rng;
    std::atomic<juce::int64> pendingSeed { -1 };
    Scene scene;
    std::array<float, numBeds> bedLevel {};
    float bedSmoothing = 0.0f;

    // Flächen
    OnePole hissHP[2], hissLP[2], crackleHP[2], popLP[2], rainLP[2], rumbleLP[2][2], rumbleHP[2];
    float crackleEnv[2] {}, crackleSign[2] { 1.0f, 1.0f }, crackleDecay = 0.0f, popEnv[2] {}, popDecay = 0.0f;
    double recordPhase = 0.0;
    float pinkState[2][3] {};
    juce::dsp::StateVariableTPTFilter<float> windBand[2][2];
    float windFreq[2][2] {}, windTarget[2][2] {}, windSwell = 0.5f, windSwellTarget = 0.5f;
    int controlCounter = 0;
    std::array<Drop, 12> drops;
    float rumbleSwell = 0.5f, rumbleSwellTarget = 0.5f;
    double swPhase = 0.0;
    float swFreq = 900.0f, swTarget = 900.0f, swFade = 0.0f, swFadeTarget = 0.0f;
    juce::dsp::StateVariableTPTFilter<float> swNoise;

    // Ereignisse
    std::array<Voice, 10> voices;
    double stepPhase = 0.0;          // interner Takt in 16teln, wenn der Host steht
    juce::int64 lastPpqStep = -1;

    // Dub-Delay, Klangfarbe, Hall
    juce::AudioBuffer<float> delayBuf;
    int delayWrite = 0;
    float delayTimeL = 0.0f, delayTimeR = 0.0f, feedbackNow = 0.5f, verbNow = 0.5f, widthNow = 1.0f;
    OnePole delayHP[2], delayLP[2], toneLP[2], outHP[2];
    float toneNow = 9000.0f;

    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> verbBuf;

    std::array<float, numElements> shown {};
};
