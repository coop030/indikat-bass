// Rendert jedes Synth-Preset mit einer typischen Minimal-Dub-Bassline (124 BPM, 4 Takte)
// und prüft auf NaN/Inf und Übersteuerung. Ausgabe: WAV-Dateien im angegebenen Ordner.
#include "../Source/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    constexpr double sr = 48000.0, bpm = 124.0;
    constexpr int block = 256;
    const double samplesPerStep = sr * 60.0 / bpm / 4.0; // 1/16

    // Pattern in 16teln: { Step, Note, Länge in Steps, Velocity }
    struct Hit { int step, note, len; float vel; };
    const Hit pattern[] = {
        { 2, 36, 2, 0.9f }, { 6, 36, 1, 0.7f }, { 7, 43, 2, 0.8f }, { 10, 36, 2, 1.0f }, { 14, 39, 1, 0.6f },
        { 15, 41, 2, 0.75f }, // überlappt Takt-Ende leicht -> Glide/Legato hörbar
    };
    const int bars = 4, totalSteps = bars * 16;
    const int totalSamples = (int) (samplesPerStep * totalSteps + sr * 1.5);

    bool allOk = true;
    DubBassProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);

    for (int p = 0; p < proc.getNumPrograms(); ++p)
    {
        proc.setCurrentProgram (p);
        proc.prepareToPlay (sr, block);

        juce::MidiBuffer all;
        for (int bar = 0; bar < bars; ++bar)
            for (auto& h : pattern)
            {
                const int on = (int) ((bar * 16 + h.step) * samplesPerStep);
                const int off = (int) ((bar * 16 + h.step + h.len) * samplesPerStep) + (h.step == 15 ? 200 : -200);
                all.addEvent (juce::MidiMessage::noteOn (1, h.note, h.vel), on);
                all.addEvent (juce::MidiMessage::noteOff (1, h.note), off);
            }

        juce::AudioBuffer<float> out (2, totalSamples);
        out.clear();
        float peak = 0.0f; double sumSq = 0.0; bool bad = false;

        for (int pos = 0; pos < totalSamples; pos += block)
        {
            const int n = juce::jmin (block, totalSamples - pos);
            juce::AudioBuffer<float> buf (2, n);
            buf.clear();
            juce::MidiBuffer midi;
            for (const auto m : all)
                if (m.samplePosition >= pos && m.samplePosition < pos + n)
                    midi.addEvent (m.getMessage(), m.samplePosition - pos);
            proc.processBlock (buf, midi);

            for (int ch = 0; ch < 2; ++ch)
            {
                out.copyFrom (ch, pos, buf, ch, 0, n);
                for (int i = 0; i < n; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (! std::isfinite (v)) bad = true;
                    peak = juce::jmax (peak, std::abs (v));
                    sumSq += (double) v * v;
                }
            }
        }

        const double rmsDb = juce::Decibels::gainToDecibels (std::sqrt (sumSq / (2.0 * totalSamples)));
        const bool ok = ! bad && peak > 0.01f && peak < 2.0f;
        allOk &= ok;
        std::printf ("%-2d %-22s peak %6.2f dBFS  rms %6.1f dBFS  %s\n", p + 1, proc.getProgramName (p).toRawUTF8(),
                     juce::Decibels::gainToDecibels (peak), rmsDb, ok ? "OK" : "PROBLEM");

        // Normalisiert auf -1 dBFS schreiben, damit alle Demos gleich laut sind
        out.applyGain (juce::Decibels::decibelsToGain (-1.0f) / juce::jmax (peak, 1.0e-6f));
        auto file = outDir.getChildFile (juce::String (p + 1).paddedLeft ('0', 2) + " " + proc.getProgramName (p) + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
            }
    }
    return allOk ? 0 : 1;
}
