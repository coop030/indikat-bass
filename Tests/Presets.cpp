// Spielt jedes Preset mit dem echten Prozessor: 4 Pads je 4 s, misst Spitzen, Übersteuerung und lauteste 50 ms
#include "../Source/PluginProcessor.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    StarPadProcessor proc;
    const double sr = 44100.0; const int block = 512;
    const int padSeq[] { 0, 3, 5, 2 };
    for (int prog = 0; prog < proc.getNumPrograms(); ++prog)
    {
        proc.setCurrentProgram (prog);
        proc.prepareToPlay (sr, block);
        juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
        float peak = 0; int hot = 0, total = 0; double win = 0; int winN = 0; float loudest = 0;
        const int blocksPerPad = (int) (sr * 4 / block);
        for (int pi = 0; pi < 4; ++pi)
        {
            proc.padRequest.store (padSeq[pi]);
            for (int b = 0; b < blocksPerPad; ++b)
            {
                proc.processBlock (buf, midi);
                for (int i = 0; i < block; ++i)
                {
                    const float v = juce::jmax (std::abs (buf.getSample (0, i)), std::abs (buf.getSample (1, i)));
                    peak = juce::jmax (peak, v); hot += v > 0.9f; ++total;
                    win += v * v; if (++winN == 2205) { loudest = juce::jmax (loudest, (float) std::sqrt (win / winN)); win = 0; winN = 0; }
                }
            }
        }
        std::printf ("%2d %-14s peak %.2f  >0.9: %5.2f %%  lautestes 50ms %.2f\n", prog + 1, proc.getProgramName (prog).toRawUTF8(),
                     peak, 100.0 * hot / total, loudest);
    }
}
