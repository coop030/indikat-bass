// Stresstest: schnelle Pad-Wechsel, MIDI, Parameter-Sprünge bei 44.1 kHz; sucht Sprünge und Spitzen im Ausgang
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/StarPadEngine.h"

int main (int argc, char** argv)
{
    const double sr = 44100.0;
    const int seconds = argc > 1 ? std::atoi (argv[1]) : 180;
    juce::Random r (argc > 2 ? std::atoi (argv[2]) : 1);
    StarPadEngine e;
    StarPadParams p; p.stars = 1.0f; p.starHeight = 3; p.starLevel = 0.8f; p.delayMix = 0.45f; p.feedback = 0.65f;
    e.setParams (p); e.prepare (sr); e.setParams (p);

    const bool sparksOnly = argc > 3 && juce::String (argv[3]) == "sparks";
    if (sparksOnly)
    {
        p.cutoff = 100; p.sub = 0; p.reso = 0; p.lfoDepth = 0; p.delayMix = 0; p.feedback = 0; p.revMix = 0; p.noise = 0;
        p.stars = 1; p.starHeight = 3; p.starLevel = 1; p.chorus = 0;
        e.setParams (p); e.setPad (0);
    }
    const int total = (int) (sr * seconds);
    std::vector<float> L ((size_t) total), R ((size_t) total);
    double ppq = 0.0; int nextEvent = 0;
    for (int pos = 0; pos < total; )
    {
        if (! sparksOnly && pos >= nextEvent)
        {
            const float x = r.nextFloat();
            if (x < 0.35f) e.setPad (r.nextInt (9) - 1);
            else if (x < 0.55f) { int n = 36 + r.nextInt (36); e.noteOn (n, r.nextFloat()); }
            else if (x < 0.65f) { for (int n = 0; n < 128; ++n) e.noteOff (n); }
            else
            {
                p.wave = r.nextInt (4); p.voices = 1 + r.nextInt (7); p.detune = r.nextFloat() * 50;
                p.pulse = r.nextFloat() < 0.4f ? r.nextInt (5) : 0; p.attack = 0.002f + r.nextFloat() * r.nextFloat() * 4;
                p.release = 0.05f + r.nextFloat() * r.nextFloat() * 7; p.gate = 0.03f + r.nextFloat() * 0.5f;
                p.cutoff = 100 + r.nextFloat() * r.nextFloat() * 7900; p.reso = r.nextFloat(); p.lfoDepth = r.nextFloat();
                p.lfoRate = r.nextInt (6); p.chordSet = r.nextInt (3); p.voicing = r.nextInt (2); p.chordOn = r.nextBool();
                p.octave = r.nextInt (3) - 1; p.sub = r.nextFloat(); p.feedback = r.nextFloat() * 0.9f;
                p.delayMix = r.nextFloat(); p.revMix = r.nextFloat(); p.revSize = r.nextFloat(); p.stars = r.nextFloat();
                p.starHeight = 1 + r.nextInt (3); p.starLevel = r.nextFloat(); p.chorus = r.nextFloat(); p.delayDiv = r.nextInt (4);
                e.setParams (p);
            }
            nextEvent = pos + (int) (sr * (0.05 + r.nextFloat() * 1.5));
        }
        const int n = juce::jmin (64 + r.nextInt (960), total - pos);
        e.process (L.data() + pos, R.data() + pos, n, 122.0, ppq);
        ppq += n * 122.0 / 60.0 / sr;
        pos += n;
    }

    // Größte Sprünge zwischen Samples
    struct Hit { float jump; int at; };
    std::vector<Hit> hits;
    int nans = 0;
    for (int i = 1; i < total; ++i)
    {
        if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) { ++nans; continue; }
        const float j = juce::jmax (std::abs (L[(size_t) i] - L[(size_t) i - 1]), std::abs (R[(size_t) i] - R[(size_t) i - 1]));
        if (j > 0.25f) hits.push_back ({ j, i });
    }
    std::sort (hits.begin(), hits.end(), [] (auto& a, auto& b) { return a.jump > b.jump; });
    std::printf ("samples %d  nan %d  jumps>0.25: %d\n", total, nans, (int) hits.size());
    for (size_t k = 0; k < juce::jmin<size_t> (8, hits.size()); ++k)
        std::printf ("  jump %.3f at %.3f s\n", hits[k].jump, hits[k].at / sr);

    juce::AudioBuffer<float> out (2, total);
    out.copyFrom (0, 0, L.data(), total); out.copyFrom (1, 0, R.data(), total);
    auto f = juce::File::getCurrentWorkingDirectory().getChildFile ("demos/stress.wav");
    f.deleteFile();
    juce::WavAudioFormat wav;
    if (auto s = std::unique_ptr<juce::FileOutputStream> (f.createOutputStream()))
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (s.get(), sr, 2, 24, {}, 0)))
        { s.release(); w->writeFromAudioSampleBuffer (out, 0, total); }
    return 0;
}
