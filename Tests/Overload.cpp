// Übersteuerungstest: typische "zu viel auf einmal"-Situationen aus der DAW.
// Misst Spitze, Anteil Samples über 0.9 (= Sättigung greift hart) und lautestes 50-ms-Fenster.
#include <juce_core/juce_core.h>
#include "../Source/StarPadEngine.h"

namespace
{
    struct Scene { const char* name; StarPadParams p; int pad; std::vector<int> notes; };

    void run (const Scene& s)
    {
        const double sr = 44100.0; const int block = 256;
        StarPadEngine e;
        e.setParams (s.p); e.prepare (sr); e.setParams (s.p);
        if (s.pad >= 0) e.setPad (s.pad);
        for (int n : s.notes) e.noteOn (n, 1.0f);

        std::vector<float> L (block), R (block);
        double ppq = 0.0; float peak = 0; long hot = 0, total = 0; double win = 0; int winN = 0; float loudest = 0;
        bool bad = false;
        for (int b = 0; b < (int) (sr * 8 / block); ++b)
        {
            e.process (L.data(), R.data(), block, 122.0, ppq);
            ppq += block * 122.0 / 60.0 / sr;
            for (int i = 0; i < block; ++i)
            {
                if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) bad = true;
                const float v = juce::jmax (std::abs (L[(size_t) i]), std::abs (R[(size_t) i]));
                peak = juce::jmax (peak, v); hot += v > 0.9f; ++total;
                win += v * v; if (++winN == 2205) { loudest = juce::jmax (loudest, (float) std::sqrt (win / winN)); win = 0; winN = 0; }
            }
        }
        std::printf ("%-34s peak %.2f  >0.9: %5.1f %%  lautestes 50ms %.2f%s\n", s.name, peak, 100.0 * hot / total, loudest,
                     bad ? "  NaN/Inf!" : "");
    }
}

int main()
{
    std::vector<Scene> scenes;
    StarPadParams base;

    scenes.push_back ({ "Pad allein (Referenz)", base, 0, {} });

    { auto p = base; scenes.push_back ({ "Akkord im Piano Roll (4 Töne)", p, -1, { 57, 60, 64, 67 } }); }
    { auto p = base; p.chordOn = true; scenes.push_back ({ "MIDI Chord On + Akkord im Piano Roll", p, -1, { 57, 60, 64, 67 } }); }
    { auto p = base; p.chordOn = true; p.voices = 7; p.sub = 0.6f; p.reso = 0.8f; p.lfoDepth = 0.7f;
      scenes.push_back ({ "+ 7 Voices, Sub, hohe Reso", p, -1, { 57, 60, 64, 67, 72 } }); }
    { auto p = base; p.chordOn = true; p.voices = 7; p.sub = 0.6f; p.reso = 0.8f; p.outGain = 6.0f;
      scenes.push_back ({ "+ Output +6 dB", p, 2, { 45, 57, 60, 64, 67, 72 } }); }
    { auto p = base; p.chordOn = true; p.pulse = 2; p.gate = 0.6f; p.release = 3.0f; p.feedback = 0.85f; p.delayMix = 0.8f;
      p.voices = 6; p.reso = 0.7f;
      scenes.push_back ({ "Dub-Stabs, lange Release, viel Echo", p, 0, { 57, 60, 64, 67 } }); }

    { auto p = base; p.chordOn = true; p.voices = 7; p.detune = 40; p.sub = 1.0f; p.reso = 1.0f; p.cutoff = 3000; p.outGain = 12.0f;
      p.feedback = 0.9f; p.delayMix = 1.0f; p.revMix = 1.0f; p.revSize = 1.0f; p.stars = 1.0f; p.starLevel = 1.0f;
      scenes.push_back ({ "Worst Case: alles auf Anschlag", p, 0, { 36, 45, 48, 52, 55, 57, 60, 64, 67, 72 } }); }
    { auto p = base; p.chordOn = true; p.pulse = 4; p.gate = 1.0f; p.release = 8.0f; p.attack = 0.002f; p.voices = 7;
      p.reso = 0.9f; p.cutoff = 2500; p.outGain = 6.0f;
      scenes.push_back ({ "Eighths-Stabs, Release 8 s, +6 dB", p, 0, { 45, 52, 57, 60, 64, 67 } }); }

    for (auto& s : scenes) run (s);
    return 0;
}
