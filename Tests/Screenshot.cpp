// Rendert die Oberfläche (mit gehaltenem Pad und laufendem Audio) als PNG: docs/starpad.png
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    StarPadProcessor proc;
    proc.setCurrentProgram (argc > 1 ? std::atoi (argv[1]) : 18);
    const double sr = 44100.0; const int block = 512;
    proc.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    proc.padRequest.store (0);
    for (int frame = 0; frame < 90; ++frame)
    {
        for (int b = 0; b < (int) (sr / 30 / block) + 1; ++b) proc.processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (33);
    }
    auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    auto out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 2 ? argv[2] : "docs/starpad.png");
    out.getParentDirectory().createDirectory();
    out.deleteFile();
    juce::FileOutputStream s (out);
    juce::PNGImageFormat().writeImageToStream (img, s);
    std::printf ("%s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}
