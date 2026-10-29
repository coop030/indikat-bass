// Offline-Test: rendert ein paar typische Szenen als WAV nach demos/ und prüft Pegel und NaN
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/StarPadEngine.h"

namespace
{
    struct Scene
    {
        const char* name;
        StarPadParams params;
        int pad;            // gehaltenes Pad, -1 = stattdessen MIDI-Noten
        std::vector<int> notes;
        double holdSeconds, tailSeconds;
    };

    bool render (const Scene& s, const juce::File& dir)
    {
        constexpr double sr = 48000.0;
        constexpr int block = 256;
        StarPadEngine engine;
        engine.setParams (s.params);
        engine.prepare (sr);
        engine.setParams (s.params);

        const int total = (int) ((s.holdSeconds + s.tailSeconds) * sr);
        const int holdEnd = (int) (s.holdSeconds * sr);
        juce::AudioBuffer<float> out (2, total);
        double ppq = 0.0;
        const double bpm = 122.0;

        if (s.pad >= 0) engine.setPad (s.pad);
        for (int n : s.notes) engine.noteOn (n, 0.8f);

        bool released = false;
        for (int pos = 0; pos < total; pos += block)
        {
            if (! released && pos >= holdEnd)
            {
                engine.setPad (-1);
                for (int n : s.notes) engine.noteOff (n);
                released = true;
            }
            const int n = juce::jmin (block, total - pos);
            engine.process (out.getWritePointer (0, pos), out.getWritePointer (1, pos), n, bpm, ppq);
            ppq += n * bpm / 60.0 / sr;
        }

        float peak = 0.0f;
        double sum = 0.0;
        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < total; ++i)
            {
                const float v = out.getSample (ch, i);
                if (! std::isfinite (v)) finite = false;
                peak = juce::jmax (peak, std::abs (v));
                if (i < holdEnd) sum += v * v;
            }
        const double rms = std::sqrt (sum / (2.0 * holdEnd));
        const float tail = out.getMagnitude (total - (int) sr / 2, (int) sr / 2);

        auto file = dir.getChildFile (juce::String (s.name) + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0)))
            {
                stream.release();
                w->writeFromAudioSampleBuffer (out, 0, total);
            }

        std::printf ("%-16s peak %.3f  rms %.1f dBFS  tail %.4f  %s\n", s.name, peak,
                     juce::Decibels::gainToDecibels ((float) rms), tail, finite ? "ok" : "NaN!");
        return finite && peak > 0.01f;
    }
}

int main()
{
    auto dir = juce::File::getCurrentWorkingDirectory().getChildFile ("demos");
    dir.createDirectory();

    StarPadParams pad;                                   // Default ~ Andromeda
    StarPadParams dub;  dub.voices = 3; dub.detune = 8; dub.attack = 0.005f; dub.release = 0.35f; dub.cutoff = 1100;
    dub.reso = 0.63f; dub.chordSet = 1; dub.voicing = 1; dub.pulse = 2; dub.gate = 0.12f; dub.feedback = 0.65f;
    dub.delayMix = 0.45f; dub.revMix = 0.35f; dub.revSize = 0.58f; dub.noise = 0.15f; dub.sub = 0.0f;
    StarPadParams stars = pad; stars.wave = 3; stars.voices = 3; stars.cutoff = 5000; stars.stars = 0.9f;
    stars.starHeight = 3; stars.revMix = 0.7f; stars.revSize = 0.85f;
    StarPadParams wall = pad; wall.voices = 7; wall.detune = 35; wall.cutoff = 2800; wall.stars = 1.0f; wall.starLevel = 0.8f;
    wall.delayMix = 0.45f; wall.feedback = 0.65f; wall.revMix = 0.6f; wall.chorus = 0.8f;
    StarPadParams midiChord = pad; midiChord.chordOn = true;

    const Scene scenes[] = {
        { "pad_am9", pad, 0, {}, 8.0, 5.0 },
        { "dub_stabs", dub, 0, {}, 8.0, 5.0 },
        { "stardust", stars, 2, {}, 8.0, 5.0 },
        { "supernova", wall, 4, {}, 8.0, 5.0 },
        { "midi_chord", midiChord, -1, { 57, 62 }, 6.0, 5.0 },
        { "midi_poly", pad, -1, { 45, 52, 55, 60, 64 }, 6.0, 5.0 },
    };

    bool ok = true;
    for (auto& s : scenes) ok &= render (s, dir);
    return ok ? 0 : 1;
}
