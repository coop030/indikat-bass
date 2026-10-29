#pragma once

#include <juce_dsp/juce_dsp.h>

// Dunkler, modulierter FDN-Hall (8 Leitungen, Householder-Matrix) mit Eingangs-Diffusion.
// Size steuert Raumgröße und Nachhallzeit, Tone die Dämpfung, optional Low Cut am Eingang.
class DubReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double maxScale = 1.7;
        for (int i = 0; i < numLines; ++i)
        {
            lines[i].assign ((size_t) (baseMs[i] * 0.001 * sr * maxScale) + 64, 0.0f);
            writePos[i] = 0;
            damp[i] = 0.0f;
            modPhase[i] = (float) i * 0.77f;
        }
        for (int i = 0; i < numDiffusers; ++i)
        {
            diffusers[i].assign ((size_t) (diffMs[i] * 0.001 * sr) + 1, 0.0f);
            diffPos[i] = 0;
        }
        inputHP.prepare ({ sampleRate, 512, 1 });
        inputHP.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        inputHP.setCutoffFrequency (220.0f);
        energy = 0.0f;
    }

    void setParameters (float size, float tone, bool lowCut)
    {
        // Size 0..1 -> Raumskalierung 0.55..1.6 und T60 0.7..14 s
        scale = 0.55f + 1.05f * size;
        const float t60 = 0.7f * std::pow (20.0f, size);
        for (int i = 0; i < numLines; ++i)
        {
            const float lenSec = baseMs[i] * 0.001f * scale;
            gain[i] = std::pow (10.0f, -3.0f * lenSec / t60);
        }
        // Tone 0..1 -> Dämpfungs-Grenzfrequenz 1.2..12 kHz (one-pole)
        const float fc = 1200.0f * std::pow (10.0f, tone);
        dampCoef = std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sr);
        useLowCut = lowCut;
    }

    // Verarbeitet ein Stereo-Sample und liefert das reine Hallsignal (wet)
    void process (float inL, float inR, float& outL, float& outR)
    {
        float x = 0.5f * (inL + inR);
        if (useLowCut)
            x = inputHP.processSample (0, x);

        // Eingangs-Diffusion (Allpass-Kette)
        for (int i = 0; i < numDiffusers; ++i)
        {
            auto& buf = diffusers[i];
            const float delayed = buf[(size_t) diffPos[i]];
            const float v = x + 0.62f * delayed;
            buf[(size_t) diffPos[i]] = v;
            x = delayed - 0.62f * v;
            if (++diffPos[i] >= (int) buf.size()) diffPos[i] = 0;
        }

        // Leitungen lesen (moduliert, interpoliert), dämpfen
        float out[numLines];
        float sum = 0.0f;
        for (int i = 0; i < numLines; ++i)
        {
            modPhase[i] += modInc[i] / (float) sr;
            if (modPhase[i] > juce::MathConstants<float>::twoPi) modPhase[i] -= juce::MathConstants<float>::twoPi;
            const float len = baseMs[i] * 0.001f * (float) sr * scale + 6.0f * std::sin (modPhase[i]);

            const auto& buf = lines[i];
            const int size = (int) buf.size();
            float rp = (float) writePos[i] - len;
            while (rp < 0.0f) rp += (float) size;
            const int i0 = (int) rp;
            const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
            const float frac = rp - (float) i0;
            float v = buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);

            damp[i] = v + dampCoef * (damp[i] - v);
            out[i] = damp[i] * gain[i];
            sum += out[i];
        }

        // Householder-Rückkopplung + Einspeisung
        const float h = sum * (2.0f / numLines);
        for (int i = 0; i < numLines; ++i)
        {
            auto& buf = lines[i];
            buf[(size_t) writePos[i]] = out[i] - h + x * inSign[i] * 0.35f;
            if (++writePos[i] >= (int) buf.size()) writePos[i] = 0;
        }

        outL = (out[0] - out[2] + out[4] - out[6] + 0.5f * (out[1] + out[5])) * 0.5f;
        outR = (out[1] - out[3] + out[5] - out[7] + 0.5f * (out[2] + out[6])) * 0.5f;
        energy += 0.0005f * ((outL * outL + outR * outR) - energy);
    }

    float getEnergy() const { return energy; }

private:
    static constexpr int numLines = 8, numDiffusers = 4;
    static constexpr float baseMs[numLines] { 31.7f, 37.1f, 41.9f, 47.3f, 53.9f, 59.3f, 67.1f, 73.7f };
    static constexpr float diffMs[numDiffusers] { 4.7f, 3.6f, 12.7f, 9.3f };
    static constexpr float inSign[numLines] { 1, -1, 1, -1, -1, 1, -1, 1 };
    static constexpr float modInc[numLines] { 0.53f, 0.71f, 0.89f, 0.61f, 0.97f, 0.43f, 0.79f, 0.67f }; // rad/s

    double sr = 44100.0;
    std::vector<float> lines[numLines];
    int writePos[numLines] {};
    float gain[numLines] {}, damp[numLines] {}, modPhase[numLines] {};
    std::vector<float> diffusers[numDiffusers];
    int diffPos[numDiffusers] {};
    juce::dsp::StateVariableTPTFilter<float> inputHP;
    float scale = 1.0f, dampCoef = 0.5f, energy = 0.0f;
    bool useLowCut = true;
};
