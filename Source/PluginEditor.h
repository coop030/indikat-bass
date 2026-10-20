#pragma once

#include "PluginProcessor.h"

class DubBassLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DubBassLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class DubBassEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DubBassEditor (DubBassProcessor&);
    ~DubBassEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

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
        juce::Colour colour;
        std::vector<Knob*> knobs;
        juce::Component* headerControl = nullptr;
        juce::Rectangle<int> bounds;
    };

    Knob& addKnob (const juce::String& paramId, const juce::String& name);
    void addChoice (juce::ComboBox&, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&,
                    const juce::String& paramId);
    void timerCallback() override;
    void layoutSection (Section&, juce::Rectangle<int>);

    DubBassProcessor& proc;
    DubBassLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    juce::ComboBox filterType, delayDiv, pumpRate, presetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAtt, delayDivAtt, pumpRateAtt;
    juce::ToggleButton pingPong { "Ping Pong" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pingPongAtt;

    float envShown = 0.0f, pumpShown = 1.0f, levelShown = 0.0f;
    juce::Rectangle<int> meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DubBassEditor)
};
