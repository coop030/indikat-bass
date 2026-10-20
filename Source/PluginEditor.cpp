#include "PluginEditor.h"

namespace Colours
{
    const juce::Colour bg       { 0xff121417 };
    const juce::Colour panel    { 0xff1b1e23 };
    const juce::Colour outline  { 0xff2a2f36 };
    const juce::Colour text     { 0xffd8dde3 };
    const juce::Colour dim      { 0xff7c8592 };
    const juce::Colour lowEnd   { 0xff3fb6a8 };
    const juce::Colour drive    { 0xffe0794a };
    const juce::Colour filter   { 0xffd6b84a };
    const juce::Colour delay    { 0xff5d8fe0 };
    const juce::Colour pump     { 0xffc760c2 };
    const juce::Colour master   { 0xffb8c0cc };
}

//==============================================================================
DubBassLookAndFeel::DubBassLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Colours::dim);
    setColour (juce::ComboBox::backgroundColourId, Colours::bg);
    setColour (juce::ComboBox::outlineColourId, Colours::outline);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::arrowColourId, Colours::dim);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::outline);
    setColour (juce::ToggleButton::textColourId, Colours::text);
}

void DubBassLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float arcR = radius - 3.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::outline);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Wert-Bogen (bipolare Regler starten in der Mitte)
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knopf
    const float knobR = arcR - 7.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff353a42), centre.x, centre.y - knobR,
                                             juce::Colour (0xff1f2328), centre.x, centre.y + knobR, false));
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobR + 3.0f, 3.0f, knobR * 0.5f, 1.5f);
    g.setColour (Colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void DubBassLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    g.setColour (b.getToggleState() ? accent.withAlpha (0.25f) : Colours::bg);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (b.getToggleState() ? accent : (highlighted ? Colours::dim : Colours::outline));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (b.getToggleState() ? Colours::text : Colours::dim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

//==============================================================================
DubBassEditor::DubBassEditor (DubBassProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    auto section = [this] (const juce::String& title, juce::Colour c, std::initializer_list<Knob*> ks,
                           juce::Component* header = nullptr)
    {
        Section s { title, c, ks, header, {} };
        for (auto* k : ks)
            k->slider.setColour (juce::Slider::rotarySliderFillColourId, c);
        sections.push_back (s);
    };

    section ("LOW END", Colours::lowEnd,
             { &addKnob ("lowCut", "Low Cut"), &addKnob ("monoFreq", "Mono <"),
               &addKnob ("subFreq", "Sub Freq"), &addKnob ("subGain", "Sub Boost") });

    section ("DRIVE", Colours::drive,
             { &addKnob ("drive", "Drive"), &addKnob ("driveTone", "Tone"), &addKnob ("driveMix", "Mix") });

    addChoice (filterType, filterTypeAtt, "filterType");
    section ("ENV FILTER", Colours::filter,
             { &addKnob ("cutoff", "Cutoff"), &addKnob ("reso", "Reso"), &addKnob ("envAmt", "Env Amt"),
               &addKnob ("envAttack", "Attack"), &addKnob ("envRelease", "Release") },
             &filterType);

    addChoice (delayDiv, delayDivAtt, "delayDiv");
    section ("DUB DELAY", Colours::delay,
             { &addKnob ("feedback", "Feedback"), &addKnob ("delayLowCut", "Low Cut"),
               &addKnob ("delayTone", "Tone"), &addKnob ("delayMix", "Mix") },
             &delayDiv);

    addChoice (pumpRate, pumpRateAtt, "pumpRate");
    section ("PUMP", Colours::pump,
             { &addKnob ("pumpDepth", "Depth"), &addKnob ("pumpRelease", "Release") }, &pumpRate);

    section ("MASTER", Colours::master,
             { &addKnob ("inGain", "Input"), &addKnob ("mix", "Dry/Wet"), &addKnob ("outGain", "Output") });

    pingPong.setColour (juce::ToggleButton::tickColourId, Colours::delay);
    addAndMakeVisible (pingPong);
    pingPongAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "pingPong", pingPong);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    setSize (1060, 470);
    startTimerHz (30);
}

DubBassEditor::~DubBassEditor()
{
    setLookAndFeel (nullptr);
}

DubBassEditor::Knob& DubBassEditor::addKnob (const juce::String& paramId, const juce::String& name)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    k->slider.setPopupDisplayEnabled (false, false, nullptr);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void DubBassEditor::addChoice (juce::ComboBox& box,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att,
                               const juce::String& paramId)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        box.addItemList (choice->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, box);
}

//==============================================================================
void DubBassEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);

    // Header
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("DUBBASS", 20, 10, 200, 36, juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("minimal dub house bass processor", 140, 10, 300, 36, juce::Justification::centredLeft);

    // Sections
    for (auto& s : sections)
    {
        auto r = s.bounds.toFloat();
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Colours::outline);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (s.colour);
        g.fillRoundedRectangle (r.getX() + 12.0f, r.getY() + 14.0f, 4.0f, 14.0f, 2.0f);
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (s.title, s.bounds.getX() + 22, s.bounds.getY() + 8, 160, 26, juce::Justification::centredLeft);
    }

    // Meter: Envelope, Pump, Output
    auto m = meterArea;
    auto drawBar = [&g] (juce::Rectangle<int> r, float v, juce::Colour c, const juce::String& name)
    {
        g.setColour (Colours::dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (name, r.removeFromLeft (34), juce::Justification::centredLeft);
        auto bar = r.toFloat().reduced (0.0f, 4.0f);
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (c);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, v)), 3.0f);
    };
    const int w = m.getWidth() / 3;
    drawBar (m.removeFromLeft (w).withTrimmedRight (12), envShown, Colours::filter, "ENV");
    drawBar (m.removeFromLeft (w).withTrimmedRight (12), pumpShown, Colours::pump, "PUMP");
    drawBar (m.withTrimmedRight (12), levelShown, levelShown > 0.98f ? juce::Colours::red : Colours::lowEnd, "OUT");
}

void DubBassEditor::layoutSection (Section& s, juce::Rectangle<int> r)
{
    s.bounds = r;
    auto inner = r.reduced (10, 8);
    auto header = inner.removeFromTop (28);

    if (s.headerControl != nullptr)
    {
        auto hc = header.removeFromRight (s.title == "DUB DELAY" ? 200 : 110);
        if (s.title == "DUB DELAY")
        {
            pingPong.setBounds (hc.removeFromRight (90).reduced (0, 2));
            hc.removeFromRight (8);
        }
        s.headerControl->setBounds (hc.reduced (0, 2));
    }

    inner.removeFromTop (6);
    const int knobW = inner.getWidth() / (int) s.knobs.size();
    for (auto* k : s.knobs)
    {
        auto cell = inner.removeFromLeft (knobW);
        k->label.setBounds (cell.removeFromTop (18));
        k->slider.setBounds (cell);
    }
}

void DubBassEditor::resized()
{
    auto area = getLocalBounds().reduced (14);
    auto header = area.removeFromTop (40);
    presetBox.setBounds (header.removeFromRight (200).reduced (0, 6));
    header.removeFromRight (20);
    meterArea = header.removeFromRight (420).reduced (0, 8);
    area.removeFromTop (6);

    const int gap = 10;
    auto row1 = area.removeFromTop ((area.getHeight() - gap) / 2);
    area.removeFromTop (gap);
    auto row2 = area;

    auto place = [&] (juce::Rectangle<int>& row, std::initializer_list<int> idx)
    {
        int totalKnobs = 0;
        for (int i : idx) totalKnobs += (int) sections[(size_t) i].knobs.size();
        const int avail = row.getWidth() - gap * ((int) idx.size() - 1);
        int n = 0;
        for (int i : idx)
        {
            ++n;
            auto& s = sections[(size_t) i];
            const int w = n == (int) idx.size() ? row.getWidth()
                                                : avail * (int) s.knobs.size() / totalKnobs;
            layoutSection (s, row.removeFromLeft (w));
            row.removeFromLeft (gap);
        }
    };

    place (row1, { 0, 1, 2 });
    place (row2, { 3, 4, 5 });
}

void DubBassEditor::timerCallback()
{
    envShown  = 0.7f * envShown + 0.3f * juce::jlimit (0.0f, 1.0f, proc.envDisplay.load() * 2.0f);
    pumpShown = proc.pumpDisplay.load();
    levelShown = proc.outLevel.load();
    repaint (meterArea.expanded (2));

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}
