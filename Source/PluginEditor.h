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

// Gleicher Look wie INDIKAT Bass Synth
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

class SpaceKeyboard : public juce::MidiKeyboardComponent
{
public:
    explicit SpaceKeyboard (juce::MidiKeyboardState& s) : MidiKeyboardComponent (s, horizontalKeyboard) {}

    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
};

// Fenster im Header: Sternenhimmel, Sternbild des gehaltenen Akkords, Funken der Glöckchen, Horizont = Ausgang
class StarField : public juce::Component
{
public:
    explicit StarField (StarPadProcessor&);
    void paint (juce::Graphics&) override;
    void tick();                       // vom Panel-Timer (30 Hz)
    juce::String chordText;

private:
    struct Star  { float x, y, r, phase, speed, bright; bool cool; };
    struct Flare { float x, y, size, life; bool violet; };

    StarPadProcessor& proc;
    juce::SharedResourcePointer<IndikatFonts> fonts;
    std::vector<Star> stars;
    std::vector<Flare> flares;
    std::vector<int> held;
    float level = 0.0f, time = 0.0f;
    juce::Random rng;
};

// Ein Akkord-Pad
class ChordPad : public juce::Component
{
public:
    std::function<void()> onPress, onRelease;
    juce::String chord, degree;
    int number = 1;
    bool lit = false;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onPress) onPress(); }
    void mouseUp (const juce::MouseEvent&) override   { if (onRelease) onRelease(); }

private:
    juce::SharedResourcePointer<IndikatFonts> fonts;
};

// Die Oberfläche in fester Basisgröße; der Editor skaliert sie
class IndikatPanel : public juce::Component, private juce::Timer
{
public:
    explicit IndikatPanel (StarPadProcessor&);
    ~IndikatPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static int baseWidth()  { return 1120; }
    static int baseHeight() { return 830; }

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
        std::vector<std::pair<juce::Component*, int>> headerControls;
        juce::Rectangle<int> bounds;
    };

    Knob& addKnob (const juce::String& paramId, const juce::String& name);
    void addChoice (juce::ComboBox&, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&,
                    const juce::String& paramId);
    void addSection (const juce::String& title, std::initializer_list<Knob*>,
                     std::vector<std::pair<juce::Component*, int>> header = {});
    juce::Rectangle<int> layoutSection (Section&, juce::Rectangle<int>);
    void place (juce::Rectangle<int> row, std::initializer_list<int> sectionIndices);
    void renderBackground();
    void timerCallback() override;
    void padPressed (int);
    void padReleased (int);

    StarPadProcessor& proc;
    IndikatLookAndFeel lnf;
    juce::SharedResourcePointer<IndikatFonts> fonts;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    juce::ComboBox wave, chordSet, voicing, key, pulse, lfoRate, delayDiv, presetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAtt, chordSetAtt, voicingAtt, keyAtt,
                                                                           pulseAtt, lfoRateAtt, delayDivAtt;
    juce::ToggleButton chordOn { "MIDI CHORD" }, hold { "HOLD" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chordOnAtt;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };

    StarField starField;
    ChordPad pads[StarPadEngine::numPads];
    std::unique_ptr<SpaceKeyboard> keyboard;

    juce::Image background;
    std::unique_ptr<juce::Drawable> logo;
    int lastSet = -1, lastKey = -1, lastPad = -2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IndikatPanel)
};

class StarPadEditor : public juce::AudioProcessorEditor
{
public:
    explicit StarPadEditor (StarPadProcessor&);
    void resized() override;

private:
    IndikatPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StarPadEditor)
};
