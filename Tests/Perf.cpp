// Lasttest: 7 Stimmen, langes Release, alle 0.5 s ein neuer 5-Ton-Akkord per MIDI + Pad-Wechsel
#include <juce_core/juce_core.h>
#include "../Source/StarPadEngine.h"

int main()
{
    const double sr = 44100.0; const int block = 256;
    StarPadEngine e; StarPadParams p; p.voices = 7; p.detune = 28; p.release = 7.0f; p.attack = 0.5f; p.stars = 1.0f;
    e.setParams (p); e.prepare (sr); e.setParams (p);
    std::vector<float> L (block), R (block);
    const int chords[4][5] { { 45, 52, 55, 60, 64 }, { 50, 57, 60, 65, 69 }, { 41, 48, 52, 55, 60 }, { 43, 50, 53, 57, 62 } };
    double worst = 0.0, total = 0.0; double ppq = 0; int blocks = (int) (sr * 60 / block);
    for (int b = 0; b < blocks; ++b)
    {
        if (b % (int) (sr * 0.5 / block) == 0)
        {
            const int c = (b / (int) (sr * 0.5 / block)) % 4;
            for (int n : chords[(c + 3) % 4]) e.noteOff (n);
            for (int n : chords[c]) e.noteOn (n, 0.8f);
            e.setPad (c * 2);
        }
        const auto t0 = juce::Time::getHighResolutionTicks();
        e.process (L.data(), R.data(), block, 122.0, ppq);
        const double dt = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        ppq += block * 122.0 / 60.0 / sr;
        worst = juce::jmax (worst, dt); total += dt;
    }
    const double blockDur = block / sr;
    std::printf ("CPU Schnitt %.0f %%  schlimmster Block %.0f %% der verfügbaren Zeit\n", 100 * total / (blocks * blockDur), 100 * worst / blockDur);
}
