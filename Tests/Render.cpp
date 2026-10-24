// Rendert mehrere zufällige Szenen als WAV und gibt Pegel und Elemente aus
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/PlutoEngine.h"

int main (int argc, char** argv)
{
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                      : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    const int total = (int) (sr * 24.0);
    const int numScenes = argc > 2 ? juce::String (argv[2]).getIntValue() : 8;
    juce::WavAudioFormat wav;

    // Jedes Element einzeln (nur Pegel, keine Dateien)
    if (numScenes == 0)
    {
        for (int el = 0; el < PlutoEngine::numElements; ++el)
        {
            PlutoEngine e;
            e.prepare (sr, block);
            e.soloForTest (el);
            juce::AudioBuffer<float> buf (2, total);
            buf.clear();
            double ppq = 0.0;
            for (int pos = 0; pos < total; pos += block)
            {
                const int n = juce::jmin (block, total - pos);
                e.process (buf.getWritePointer (0, pos), buf.getWritePointer (1, pos), n, 122.0, ppq, true);
                ppq += n / (sr * 60.0 / 122.0);
            }
            std::printf ("%-11s peak %6.2f dBFS  rms %6.1f dBFS\n", PlutoEngine::elementName (el),
                         juce::Decibels::gainToDecibels (buf.getMagnitude (0, total)),
                         juce::Decibels::gainToDecibels (0.5f * (buf.getRMSLevel (0, 0, total) + buf.getRMSLevel (1, 0, total))));
        }
        return 0;
    }

    for (int sceneIndex = 0; sceneIndex < numScenes; ++sceneIndex)
    {
        const auto seed = (juce::uint32) (0x9e3779b9u * (juce::uint32) (sceneIndex + 1));
        PlutoEngine e;
        e.requestScene (seed);
        e.prepare (sr, block);
        juce::AudioBuffer<float> buf (2, total);
        buf.clear();
        double ppq = 0.0;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            e.process (buf.getWritePointer (0, pos), buf.getWritePointer (1, pos), n, 122.0, ppq, true);
            ppq += n / (sr * 60.0 / 122.0);
        }

        juce::StringArray names;
        for (int i = 0; i < PlutoEngine::numElements; ++i)
            if (e.display.activeMask.load() & (1u << i))
                names.add (juce::String (PlutoEngine::elementName (i)).toLowerCase().replace (" ", "-"));

        const float peak = buf.getMagnitude (0, total);
        const float rms = 0.5f * (buf.getRMSLevel (0, 0, total) + buf.getRMSLevel (1, 0, total));
        std::printf ("%d  peak %6.2f dBFS  rms %6.1f dBFS  events %3d  %s\n", sceneIndex + 1,
                     juce::Decibels::gainToDecibels (peak), juce::Decibels::gainToDecibels (rms),
                     e.display.eventCount.load(), names.joinIntoString (" ").toRawUTF8());

        auto f = outDir.getChildFile ("pluto-" + juce::String (sceneIndex + 1).paddedLeft ('0', 2) + "-" + names.joinIntoString ("+") + ".wav");
        f.deleteFile();
        if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream().release()))
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0)))
            {
                os.release();
                w->writeFromAudioSampleBuffer (buf, 0, total);
            }
    }
    return 0;
}
