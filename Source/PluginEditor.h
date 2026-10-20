#pragma once

#include "PluginProcessor.h"

// Eingebettete Schriften (OFL)
struct IndikatFonts
{
    IndikatFonts();
    juce::Font wide (float h) const  { return juce::Font (juce::FontOptions (michroma).withHeight (h)); }
    juce::Font label (float h) const { return juce::Font (juce::FontOptions (questrial).withHeight (h)); }
    juce::Font mono (float h) const  { return juce::Font (juce::FontOptions (shareTech).withHeight (h)); }

    juce::Typeface::Ptr michroma, questrial, shareTech;
};

class IndikatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IndikatLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

private:
    juce::SharedResourcePointer<IndikatFonts> fonts;
};

// Kreisförmige Wellenform-Darstellung („Orbit“)
class Orbit : public juce::Component
{
public:
    explicit Orbit (DubBassProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    DubBassProcessor& proc;
};

// Minimalistische Tastatur
class SpaceKeyboard : public juce::MidiKeyboardComponent
{
public:
    explicit SpaceKeyboard (juce::MidiKeyboardState& s) : MidiKeyboardComponent (s, horizontalKeyboard) {}

    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
};

// Die Oberfläche in fester Basisgröße; der Editor skaliert sie
class IndikatPanel : public juce::Component, private juce::Timer
{
public:
    explicit IndikatPanel (DubBassProcessor&);
    ~IndikatPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static int baseWidth()  { return 1120; }
    static int baseHeight() { return DubBassProcessor::isSynth ? 790 : 530; }

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        std::vector<Knob*> knobs;
        std::vector<std::pair<juce::Component*, int>> headerControls; // Komponente + Breite
        juce::Rectangle<int> bounds;
    };

    Knob& addKnob (const juce::String& paramId, const juce::String& name);
    void addChoice (juce::ComboBox&, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&,
                    const juce::String& paramId);
    void addSection (const juce::String& title, std::initializer_list<Knob*>,
                     std::vector<std::pair<juce::Component*, int>> header = {});
    void layoutSection (Section&, juce::Rectangle<int>);
    void place (juce::Rectangle<int> row, std::initializer_list<int> sectionIndices);
    void renderBackground();
    void drawMeter (juce::Graphics&, juce::Rectangle<int>, const juce::String& name, float value) const;
    void timerCallback() override;

    DubBassProcessor& proc;
    IndikatLookAndFeel lnf;
    juce::SharedResourcePointer<IndikatFonts> fonts;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    juce::ComboBox filterType, delayDiv, pumpRate, presetBox, osc1Wave, osc2Wave, legato;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAtt, delayDivAtt, pumpRateAtt,
                                                                           osc1WaveAtt, osc2WaveAtt, legatoAtt;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };
    juce::ToggleButton pingPong { "PING PONG" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pingPongAtt;

    Orbit orbit;
    std::unique_ptr<SpaceKeyboard> keyboard;

    juce::Image background;
    std::unique_ptr<juce::Drawable> logo;
    juce::Rectangle<int> meterArea, pumpDot;
    float envShown = 0.0f, levelShown = 0.0f, pumpShown = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IndikatPanel)
};

class DubBassEditor : public juce::AudioProcessorEditor
{
public:
    explicit DubBassEditor (DubBassProcessor&);
    void resized() override;

private:
    IndikatPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DubBassEditor)
};
