#pragma once

#include "PluginProcessor.h"

// Echtzeit-Visualisierung: Schwarzes Loch mit Gravitationslinse und Akkretionsscheibe.
// Das Aussehen folgt den Parametern, jede tatsächliche Parameteränderung löst einen Effekt aus
// (Partikel, Schockwelle, Glitch, Echo-Wellen, Masse-Kick je nach Parametergruppe).
class BlackHole : public juce::Component, private juce::Timer
{
public:
    explicit BlackHole (DubBassProcessor&);
    ~BlackHole() override;

    void paint (juce::Graphics&) override;

private:
    enum class Group { Mass, Filter, Drive, Echo, Pump, Voice };

    struct Particle
    {
        float r, a, w, life;
        juce::Colour colour;
        juce::Point<float> prev;
        bool hasPrev = false;
    };

    struct Wave
    {
        float age, strength;
        juce::Colour colour;
    };

    void timerCallback() override;
    void detectParameterChanges();
    void updateVisualState (float dt);
    void ensureRaster();
    void renderRaster();
    void buildColourMap();
    float norm (const char* id) const;
    juce::Point<float> toPixels (float x, float y) const;

    DubBassProcessor& proc;
    juce::Random rng { 0x5eed };

    // Parameter-Überwachung
    struct Watched { juce::RangedAudioParameter* param; float last; Group group; };
    std::vector<Watched> watched;

    // Raster
    juce::Image raster;
    int rw = 0, rh = 0;
    float rasterScale = 1.0f;
    std::vector<float> stars, planeR, planeG, planeB;
    std::vector<float> diskTex;                 // 512 Winkel x 64 Radien
    std::array<float, 256 * 3> colourMap {};
    std::array<float, 512> gaussTable {};

    // Dynamik
    double lastTime = 0.0;
    float time = 0.0f, spinPhase = 0.0f;
    float rs = 0.4f, rsVel = 0.0f;
    float flare = 0.0f, glitch = 0.0f, horizonFlash = 0.0f, breathKick = 0.0f, spinBoost = 0.0f, burstFlash = 0.0f;
    float waveCooldown = 0.0f;

    // Geglättete Darstellungswerte
    float heat = 0.5f, crunch = 0.0f, ringAmt = 0.5f, ringWidth = 0.06f, tilt = 0.25f;
    float echoCount = 0.0f, echoAmt = 0.0f, echoDecay = 0.5f, wobble = 0.0f, binary = 0.0f;
    float diskAmt = 1.0f, level = 0.0f, envFlare = 0.0f, duck = 0.0f, rout = 3.4f;
    int texStyle = 2;
    juce::Point<float> wobbleOffset;

    std::vector<Particle> particles;
    std::vector<Wave> waves;

    juce::String hudName, hudValue;
    float hudAlpha = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlackHole)
};
