#pragma once

#include "PluginProcessor.h"

// Eingebettete Schriften (OFL)
struct IndikatFonts
{
    IndikatFonts();
    juce::Font wide (float h) const  { return make (michroma, h); }
    juce::Font label (float h) const { return make (questrial, h); }
    juce::Font mono (float h) const  { return make (shareTech, h); }

    juce::Typeface::Ptr michroma, questrial, shareTech;

private:
    static juce::Font make (const juce::Typeface::Ptr& t, float h)
    {
       #if JUCE_MAJOR_VERSION >= 8
        return juce::Font (juce::FontOptions (t).withHeight (h));
       #else
        return juce::Font (t).withHeight (h);   // JUCE 7: für den Windows-Build mit MinGW
       #endif
    }
};

// Der Planet ist der einzige Bedienknopf: Klick = neue Zufallsszene
class PlanetButton : public juce::Component
{
public:
    PlanetButton();

    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    float radius() const { return (float) getWidth() * 0.5f - 22.0f; }

    std::function<void()> onClick;
    double spin = 0.0;

private:
    bool pressed = false;
};

// Die Oberfläche in fester Basisgröße; der Editor skaliert sie
class IndikatPanel : public juce::Component, private juce::Timer
{
public:
    explicit IndikatPanel (PlutoProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

    static int baseWidth()  { return 560; }
    static int baseHeight() { return 700; }

private:
    struct Spark
    {
        juce::Point<float> pos;
        int type = 0;
        double born = 0.0;
    };

    void renderBackground();
    void drawSparks (juce::Graphics&, double now) const;
    void drawChip (juce::Graphics&, juce::Rectangle<int>, int element) const;
    void timerCallback() override;

    PlutoProcessor& proc;
    juce::SharedResourcePointer<IndikatFonts> fonts;

    PlanetButton planet;

    juce::Image background;
    std::unique_ptr<juce::Drawable> logo;
    juce::Rectangle<int> layerBox;
    juce::Point<float> centre;

    std::vector<Spark> sparks;
    int lastEventCount = 0;
    juce::Random rng;
    std::array<float, PlutoEngine::numElements> activity {};
    juce::uint32 activeMask = 0, shownSeed = 0;
    double lastTick = 0.0, clickTime = -10.0;
    float spinBoost = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IndikatPanel)
};

class PlutoEditor : public juce::AudioProcessorEditor
{
public:
    explicit PlutoEditor (PlutoProcessor&);
    void resized() override;

private:
    IndikatPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlutoEditor)
};
