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

// Auswahlfelder und Menüs im Indikat-Look
class IndikatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IndikatLookAndFeel();

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;

private:
    juce::SharedResourcePointer<IndikatFonts> fonts;
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
    double spin = -1.1;   // Start: das Herz auf der Lichtseite

private:
    // Vorberechnete Kugelgeometrie pro Pixel
    struct SpherePixel { float u = 0, v = 0, dot = 0, dotLon = 0, dotLat = 0, limb = 0, haze = 0, edge = 0, halo = 0; };
    static constexpr float haloScale = 1.12f;
    void prepareSphere (int size);

    std::vector<SpherePixel> sphere;
    int sphereSize = 0;
    juce::Image frame;
    float frameShift = -1.0f, frameGlow = -1.0f;   // womit das Bild zuletzt gerechnet wurde
    bool pressed = false;
};

// Die Oberfläche in fester Basisgröße; der Editor skaliert sie
class IndikatPanel : public juce::Component, private juce::Timer
{
public:
    explicit IndikatPanel (PlutoProcessor&);
    ~IndikatPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static int baseWidth()  { return 560; }
    static int baseHeight() { return 760; }

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
    IndikatLookAndFeel lnf;
    juce::SharedResourcePointer<IndikatFonts> fonts;

    PlanetButton planet;
    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;

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
