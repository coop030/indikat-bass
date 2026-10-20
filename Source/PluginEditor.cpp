#include "PluginEditor.h"
#include "BinaryData.h"

namespace Palette
{
    const juce::Colour space0  { 0xff04060a };
    const juce::Colour space1  { 0xff0a111d };
    const juce::Colour hair    { 0xff1a2333 };
    const juce::Colour hairHi  { 0xff2b384c };
    const juce::Colour text    { 0xffd2dbe8 };
    const juce::Colour dim     { 0xff66738a };
    const juce::Colour faint   { 0xff38455a };
    const juce::Colour accent  { 0xff7fe6ff };   // Eis-Cyan
    const juce::Colour counter { 0xffa292ff };   // negativer Wert bei bipolaren Reglern
}

namespace
{
    void strokeGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width)
    {
        g.setColour (c.withAlpha (0.06f));
        g.strokePath (p, juce::PathStrokeType (width * 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (0.14f));
        g.strokePath (p, juce::PathStrokeType (width * 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void glowDot (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col)
    {
        g.setColour (col.withMultipliedAlpha (0.08f));
        g.fillEllipse (juce::Rectangle<float> (r * 6.0f, r * 6.0f).withCentre (c));
        g.setColour (col.withMultipliedAlpha (0.2f));
        g.fillEllipse (juce::Rectangle<float> (r * 3.0f, r * 3.0f).withCentre (c));
        g.setColour (col);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    }
}

//==============================================================================
IndikatFonts::IndikatFonts()
{
    michroma  = juce::Typeface::createSystemTypefaceFor (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
    questrial = juce::Typeface::createSystemTypefaceFor (BinaryData::QuestrialRegular_ttf, BinaryData::QuestrialRegular_ttfSize);
    shareTech = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
}

//==============================================================================
IndikatLookAndFeel::IndikatLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Palette::accent.withAlpha (0.3f));
    setColour (juce::Label::textColourId, Palette::dim);
    setColour (juce::Label::textWhenEditingColourId, Palette::text);
    setColour (juce::TextEditor::backgroundColourId, Palette::space1);
    setColour (juce::TextEditor::outlineColourId, Palette::hairHi);
    setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent);
    setColour (juce::CaretComponent::caretColourId, Palette::accent);
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::PopupMenu::backgroundColourId, Palette::space1);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::ResizableWindow::backgroundColourId, Palette::space0);
}

void IndikatLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    const float size = (float) juce::jmin (w, h);
    const auto centre = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float outer = size * 0.5f - 4.0f;
    const float ring = outer * 0.80f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool active = s.isMouseOverOrDragging();

    auto pointOn = [&] (float radius, float a) { return centre.getPointOnCircumference (radius, a); };

    // Skala: feine Punkte außen
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const bool major = i == 0 || i == 5 || i == 10;
        const float r = major ? 1.1f : 0.7f;
        g.setColour ((i == 0 || i == 10) ? Palette::dim : Palette::faint);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pointOn (outer, a)));
    }

    // Umlaufbahn
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, ring, ring, 0.0f, startAngle, endAngle, true);
    g.setColour (active ? Palette::hairHi : Palette::hair);
    g.strokePath (track, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Wertbogen
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    const auto colour = (bipolar && angle < from) ? Palette::counter : Palette::accent;

    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, ring, ring, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        strokeGlow (g, arc, colour.withMultipliedAlpha (active ? 1.0f : 0.85f), 2.0f);
    }

    // Kern
    const float core = ring * 0.62f;
    g.setGradientFill (juce::ColourGradient (Palette::space1.brighter (0.15f), centre.x, centre.y - core,
                                             Palette::space0, centre.x, centre.y + core, false));
    g.fillEllipse (juce::Rectangle<float> (core * 2.0f, core * 2.0f).withCentre (centre));
    g.setColour (active ? Palette::hairHi : Palette::hair);
    g.drawEllipse (juce::Rectangle<float> (core * 2.0f, core * 2.0f).withCentre (centre), 1.0f);

    // Zeiger im Kern
    g.setColour (active ? Palette::text : Palette::dim);
    g.drawLine (juce::Line<float> (pointOn (core * 0.35f, angle), pointOn (core * 0.8f, angle)), 1.5f);

    // Satellit
    glowDot (g, pointOn (ring, angle), active ? 3.6f : 3.0f, colour);
}

juce::Label* IndikatLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (fonts->mono (12.5f));
    l->setColour (juce::Label::textColourId, Palette::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

void IndikatLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;

    g.setColour (on ? Palette::accent.withAlpha (0.08f) : juce::Colours::transparentBlack);
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? Palette::accent.withAlpha (0.6f) : (highlighted ? Palette::hairHi : Palette::hair));
    g.drawRoundedRectangle (r, rad, 1.0f);

    const juce::Point<float> dot (r.getX() + rad + 2.0f, r.getCentreY());
    if (on) glowDot (g, dot, 2.2f, Palette::accent);
    else    { g.setColour (Palette::faint); g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (dot)); }

    g.setColour (on ? Palette::text : Palette::dim);
    g.setFont (fonts->mono (11.0f).withExtraKerningFactor (0.12f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (rad * 2.0f + 2.0f).withTrimmedRight (rad * 0.6f),
                juce::Justification::centred);
}

void IndikatLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    if (highlighted || down)
    {
        g.setColour (Palette::accent.withAlpha (down ? 0.12f : 0.05f));
        g.fillEllipse (b.getLocalBounds().toFloat().reduced (2.0f));
    }
}

void IndikatLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool)
{
    g.setColour (highlighted ? Palette::accent : Palette::dim);
    g.setFont (fonts->mono (15.0f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void IndikatLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const bool hot = box.isMouseOver (true) || box.isPopupActive();
    g.setColour (hot ? Palette::accent.withAlpha (0.7f) : Palette::hairHi);
    g.fillRect (0.0f, (float) h - 1.0f, (float) w, 1.0f);

    juce::Path chevron;
    const float cx = (float) w - 7.0f, cy = (float) h * 0.5f;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (hot ? Palette::accent : Palette::dim);
    g.strokePath (chevron, juce::PathStrokeType (1.2f));
}

juce::Font IndikatLookAndFeel::getComboBoxFont (juce::ComboBox&) { return fonts->mono (13.0f); }

void IndikatLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (0, 0, box.getWidth() - 16, box.getHeight() - 1);
    label.setFont (getComboBoxFont (box));
    label.setBorderSize ({ 0, 2, 0, 0 });
}

void IndikatLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Palette::space1);
    g.setColour (Palette::hairHi);
    g.drawRect (0, 0, w, h, 1);
}

void IndikatLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool,
                                            bool isHighlighted, bool isTicked, bool, const juce::String& text,
                                            const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (Palette::hair);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    auto r = area.reduced (1, 0);
    if (isHighlighted)
    {
        g.setColour (Palette::accent.withAlpha (0.07f));
        g.fillRect (r);
    }
    if (isTicked)
        glowDot (g, { (float) r.getX() + 10.0f, (float) r.getCentreY() }, 2.0f, Palette::accent);

    g.setColour (isHighlighted || isTicked ? Palette::accent : Palette::text);
    g.setFont (getPopupMenuFont());
    g.drawText (text, r.withTrimmedLeft (22), juce::Justification::centredLeft);
}

juce::Font IndikatLookAndFeel::getPopupMenuFont() { return fonts->mono (14.0f); }

juce::Font IndikatLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

//==============================================================================
//==============================================================================
void SpaceKeyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                                   juce::Colour, juce::Colour)
{
    g.setGradientFill (juce::ColourGradient (Palette::space1, 0.0f, area.getY(),
                                             Palette::space0, 0.0f, area.getBottom(), false));
    g.fillRect (area);

    if (isDown || isOver)
    {
        g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (0.0f), 0.0f, area.getY(),
                                                 Palette::accent.withAlpha (isDown ? 0.35f : 0.08f), 0.0f, area.getBottom(), false));
        g.fillRect (area);
    }
    if (isDown)
        glowDot (g, { area.getCentreX(), area.getBottom() - 9.0f }, 2.0f, Palette::accent);

    g.setColour (Palette::hair);
    g.fillRect (area.getRight() - 1.0f, area.getY(), 1.0f, area.getHeight());

    if (note % 12 == 0)
    {
        g.setColour (Palette::faint);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText ("C" + juce::String (note / 12 - 1), area.withTrimmedBottom (4.0f).withTrimmedLeft (4.0f),
                    juce::Justification::bottomLeft);
    }
}

void SpaceKeyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    auto r = area.withTrimmedBottom (2.0f);
    g.setColour (juce::Colour (0xff020306));
    g.fillRect (r);
    g.setColour (isDown ? Palette::accent.withAlpha (0.8f) : (isOver ? Palette::hairHi.brighter (0.3f) : Palette::hairHi));
    g.drawRect (r, 1.0f);
    if (isDown)
    {
        g.setColour (Palette::accent.withAlpha (0.25f));
        g.fillRect (r.reduced (1.0f));
    }
}

//==============================================================================
IndikatPanel::IndikatPanel (DubBassProcessor& p)
    : proc (p), blackHole (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    if (auto svg = juce::XmlDocument::parse (juce::String::fromUTF8 (BinaryData::indikatlogo_svg, BinaryData::indikatlogo_svgSize)))
    {
        logo = juce::Drawable::createFromSVG (*svg);
        if (logo != nullptr)
            logo->replaceColour (juce::Colours::white, Palette::text);
    }

    if (DubBassProcessor::isSynth)
    {
        addChoice (osc1Wave, osc1WaveAtt, "osc1Wave");
        addSection ("OSC 1", { &addKnob ("osc1Oct", "OCTAVE"), &addKnob ("osc1Level", "LEVEL"), &addKnob ("subLevel", "SUB") },
                    { { &osc1Wave, 92 } });

        addChoice (osc2Wave, osc2WaveAtt, "osc2Wave");
        addSection ("OSC 2", { &addKnob ("osc2Oct", "OCTAVE"), &addKnob ("osc2Semi", "SEMI"),
                               &addKnob ("osc2Fine", "FINE"), &addKnob ("osc2Level", "LEVEL") },
                    { { &osc2Wave, 92 } });

        addSection ("AMP", { &addKnob ("ampAttack", "ATTACK"), &addKnob ("ampDecay", "DECAY"),
                             &addKnob ("ampSustain", "SUSTAIN"), &addKnob ("ampRelease", "RELEASE") });

        addChoice (legato, legatoAtt, "legato");
        addSection ("VOICE", { &addKnob ("glide", "GLIDE"), &addKnob ("velocity", "VELOCITY"), &addKnob ("drift", "DRIFT") },
                    { { &legato, 84 } });
    }

    addSection ("LOW END", { &addKnob ("lowCut", "LOW CUT"), &addKnob ("monoFreq", "MONO"),
                             &addKnob ("subFreq", "SUB FREQ"), &addKnob ("subGain", "SUB BOOST") });

    addSection ("DRIVE", { &addKnob ("drive", "DRIVE"), &addKnob ("driveTone", "TONE"), &addKnob ("driveMix", "MIX") });

    addChoice (filterType, filterTypeAtt, "filterType");
    addSection ("FILTER", { &addKnob ("cutoff", "CUTOFF"), &addKnob ("reso", "RESO"), &addKnob ("envAmt", "ENV"),
                            &addKnob ("envAttack", "ATTACK"),
                            &addKnob ("envRelease", DubBassProcessor::isSynth ? "DECAY" : "RELEASE") },
                { { &filterType, 100 } });

    addChoice (delayDiv, delayDivAtt, "delayDiv");
    addSection ("DUB DELAY", { &addKnob ("feedback", "FEEDBACK"), &addKnob ("delayLowCut", "LOW CUT"),
                               &addKnob ("delayTone", "TONE"), &addKnob ("delayMix", "MIX") },
                { { &delayDiv, 84 }, { &pingPong, 104 } });

    addChoice (pumpRate, pumpRateAtt, "pumpRate");
    addSection ("PUMP", { &addKnob ("pumpDepth", "DEPTH"), &addKnob ("pumpRelease", "RELEASE") }, { { &pumpRate, 64 } });

    addSection ("OUT", { &addKnob ("inGain", DubBassProcessor::isSynth ? "LEVEL" : "INPUT"),
                         &addKnob ("mix", "DRY/WET"), &addKnob ("outGain", "OUTPUT") });

    addAndMakeVisible (pingPong);
    pingPongAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "pingPong", pingPong);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    auto step = [this] (int delta)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
        repaint();
    };
    prevPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (1); };
    addAndMakeVisible (prevPreset);
    addAndMakeVisible (nextPreset);

    addAndMakeVisible (blackHole);

    if (DubBassProcessor::isSynth)
    {
        keyboard = std::make_unique<SpaceKeyboard> (proc.keyboardState);
        keyboard->setAvailableRange (24, 71);
        keyboard->setScrollButtonsVisible (false);
        keyboard->setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
        keyboard->setBlackNoteLengthProportion (0.6f);
        keyboard->setOctaveForMiddleC (4);
        addAndMakeVisible (*keyboard);
    }

    setSize (baseWidth(), baseHeight());
    startTimerHz (40);
}

IndikatPanel::~IndikatPanel()
{
    setLookAndFeel (nullptr);
}

IndikatPanel::Knob& IndikatPanel::addKnob (const juce::String& paramId, const juce::String& name)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (fonts->label (11.0f).withExtraKerningFactor (0.18f));
    k->label.setColour (juce::Label::textColourId, Palette::dim);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->slider.sendLookAndFeelChange(); // Textbox mit eigenem Look neu erzeugen
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);

    if (auto* param = proc.apvts.getParameter (paramId))
        k->slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    knobs.push_back (std::move (k));
    return *knobs.back();
}

void IndikatPanel::addChoice (juce::ComboBox& box,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att,
                              const juce::String& paramId)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        box.addItemList (choice->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, box);
}

void IndikatPanel::addSection (const juce::String& title, std::initializer_list<Knob*> ks,
                               std::vector<std::pair<juce::Component*, int>> header)
{
    sections.push_back ({ title, ks, std::move (header), {} });
}

//==============================================================================
void IndikatPanel::renderBackground()
{
    const int w = getWidth(), h = getHeight();
    background = juce::Image (juce::Image::RGB, w, h, true);
    juce::Graphics g (background);

    // Tiefer Raum mit schwachem Nebel oben rechts
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d1828), (float) w * 0.78f, (float) -h * 0.15f,
                                             Palette::space0, (float) w * 0.2f, (float) h * 1.1f, true));
    g.fillAll();

    // Sternenfeld (deterministisch)
    juce::Random rng (0x1d1ca7);
    for (int i = 0; i < 320; ++i)
    {
        const float x = rng.nextFloat() * (float) w, y = rng.nextFloat() * (float) h;
        const float bright = std::pow (rng.nextFloat(), 3.0f);
        const float r = 0.35f + bright * 0.9f;
        const auto col = rng.nextFloat() < 0.15f ? Palette::accent : juce::Colour (0xffe8eef8);
        g.setColour (col.withAlpha (0.08f + bright * 0.55f));
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
    }

    // Große, kaum sichtbare Umlaufbahnen
    const juce::Point<float> origin ((float) w * 1.02f, (float) -h * 0.25f);
    for (int i = 0; i < 4; ++i)
    {
        const float r = (float) h * (0.55f + 0.32f * (float) i);
        g.setColour (Palette::accent.withAlpha (0.035f - 0.006f * (float) i));
        g.drawEllipse (juce::Rectangle<float> (r * 2.4f, r * 2.0f).withCentre (origin), 1.0f);
    }
}

void IndikatPanel::drawMeter (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& name, float value) const
{
    g.setFont (fonts->mono (10.5f).withExtraKerningFactor (0.15f));
    g.setColour (Palette::dim);
    g.drawText (name, r.removeFromLeft (56), juce::Justification::centredLeft);

    const float y = (float) r.getCentreY();
    const float x0 = (float) r.getX(), x1 = (float) r.getRight();
    g.setColour (Palette::hair);
    g.fillRect (x0, y - 0.5f, x1 - x0, 1.0f);

    const float xv = x0 + (x1 - x0) * juce::jlimit (0.0f, 1.0f, value);
    if (xv > x0 + 0.5f)
    {
        juce::Path line;
        line.startNewSubPath (x0, y);
        line.lineTo (xv, y);
        strokeGlow (g, line, Palette::accent, 1.6f);
    }
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (Palette::faint);
        g.fillRect (x0 + (x1 - x0) * 0.5f * (float) i - 0.5f, y - 3.0f, 1.0f, 6.0f);
    }
}

void IndikatPanel::paint (juce::Graphics& g)
{
    if (background.isNull() || background.getWidth() != getWidth() || background.getHeight() != getHeight())
        renderBackground();
    g.drawImageAt (background, 0, 0);

    // Logo (Vektor, 4.93 : 1)
    if (logo != nullptr)
        logo->drawWithin (g, { 30.0f, 52.0f, 24.0f * 4.93f, 24.0f }, juce::RectanglePlacement::xLeft, 1.0f);
    glowDot (g, { 33.0f, 96.0f }, 2.2f, Palette::accent);
    g.setColour (Palette::dim);
    g.setFont (fonts->mono (11.5f).withExtraKerningFactor (0.45f));
    g.drawText (DubBassProcessor::isSynth ? "BASS SYNTH" : "BASS FX", 44, 88, 200, 16, juce::Justification::centredLeft);
    g.setColour (Palette::faint);
    g.setFont (fonts->mono (10.0f).withExtraKerningFactor (0.2f));
    g.drawText ("LF UNIT // 1.2", 31, 108, 140, 16, juce::Justification::centredLeft);

    // Preset-Nummer
    g.setColour (Palette::dim);
    g.setFont (fonts->mono (10.0f).withExtraKerningFactor (0.35f));
    g.drawText ("PROGRAM " + juce::String (proc.getCurrentProgram() + 1).paddedLeft ('0', 2)
                    + " / " + juce::String (proc.getNumPrograms()).paddedLeft ('0', 2),
                presetBox.getBounds().translated (0, -22).withHeight (16), juce::Justification::centred);

    // Trennlinie unter dem Header
    juce::ColourGradient line (Palette::hairHi.withAlpha (0.0f), 28.0f, 0.0f,
                               Palette::hairHi.withAlpha (0.0f), (float) getWidth() - 28.0f, 0.0f, false);
    line.addColour (0.15, Palette::hairHi);
    line.addColour (0.85, Palette::hairHi);
    g.setGradientFill (line);
    g.fillRect (28, 158, getWidth() - 56, 1);

    // Meter
    auto m = meterArea;
    const int rowH = m.getHeight() / 3;
    drawMeter (g, m.removeFromTop (rowH), "FILTER", envShown);
    drawMeter (g, m.removeFromTop (rowH), "PUMP", pumpShown);
    drawMeter (g, m, "OUTPUT", levelShown);

    // Sektionen
    int index = 1;
    for (auto& s : sections)
    {
        auto r = s.bounds.toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.012f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Palette::hair);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        g.setFont (fonts->mono (10.5f));
        g.setColour (Palette::accent.withAlpha (0.8f));
        g.drawText (juce::String (index++).paddedLeft ('0', 2), s.bounds.getX() + 14, s.bounds.getY() + 10, 24, 18,
                    juce::Justification::centredLeft);
        g.setFont (fonts->wide (9.0f).withExtraKerningFactor (0.3f));
        g.setColour (Palette::text.withAlpha (0.85f));
        g.drawText (s.title, s.bounds.getX() + 36, s.bounds.getY() + 10, 200, 18, juce::Justification::centredLeft);
    }

    // Pump-Puls
    if (! pumpDot.isEmpty())
    {
        const auto c = pumpDot.toFloat().getCentre();
        g.setColour (Palette::hairHi);
        g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (c), 1.0f);
        if (pumpShown > 0.02f)
            glowDot (g, c, 2.5f, Palette::accent.withAlpha (juce::jlimit (0.0f, 1.0f, pumpShown * 1.5f)));
    }

    // Rahmen der Tastatur
    if (keyboard != nullptr)
    {
        g.setColour (Palette::hair);
        g.drawRect (keyboard->getBounds().expanded (1), 1);
    }
}

void IndikatPanel::layoutSection (Section& s, juce::Rectangle<int> r)
{
    s.bounds = r;
    auto inner = r.reduced (12, 10);
    auto header = inner.removeFromTop (20);

    for (auto& [comp, width] : s.headerControls)
    {
        comp->setBounds (header.removeFromRight (width));
        header.removeFromRight (10);
    }

    if (s.title == "PUMP")
        pumpDot = juce::Rectangle<int> (s.bounds.getX() + 86, s.bounds.getY() + 14, 10, 10);

    inner.removeFromTop (8);
    const int knobW = inner.getWidth() / (int) s.knobs.size();
    for (auto* k : s.knobs)
    {
        auto cell = inner.removeFromLeft (knobW);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell.reduced (2, 0));
    }
}

void IndikatPanel::place (juce::Rectangle<int> row, std::initializer_list<int> idx)
{
    constexpr int gap = 12;
    int totalKnobs = 0;
    for (int i : idx) totalKnobs += (int) sections[(size_t) i].knobs.size();
    const int avail = row.getWidth() - gap * ((int) idx.size() - 1);

    int n = 0;
    for (int i : idx)
    {
        auto& s = sections[(size_t) i];
        const int w = ++n == (int) idx.size() ? row.getWidth() : avail * (int) s.knobs.size() / totalKnobs;
        layoutSection (s, row.removeFromLeft (w));
        row.removeFromLeft (gap);
    }
}

void IndikatPanel::resized()
{
    auto area = getLocalBounds().reduced (28, 0);

    // Header
    auto header = juce::Rectangle<int> (area.getX(), 20, area.getWidth(), 126);
    auto presetArea = header.removeFromRight (270).withTrimmedTop (62).withHeight (28);
    nextPreset.setBounds (presetArea.removeFromRight (28));
    prevPreset.setBounds (presetArea.removeFromLeft (28));
    presetBox.setBounds (presetArea.reduced (8, 0));

    header.removeFromRight (34);
    blackHole.setBounds (header.removeFromRight (316));
    header.removeFromRight (34);
    meterArea = header.removeFromRight (220).reduced (0, 36);

    // Sektionen
    area.setTop (172);
    area.removeFromBottom (24);
    if (keyboard != nullptr)
    {
        keyboard->setBounds (area.removeFromBottom (70));
        keyboard->setKeyWidth ((float) keyboard->getWidth() / 28.0f); // 4 Oktaven = 28 weiße Tasten
        area.removeFromBottom (14);
    }

    constexpr int gap = 12;
    const int numRows = DubBassProcessor::isSynth ? 3 : 2;
    const int rowH = (area.getHeight() - gap * (numRows - 1)) / numRows;
    juce::Rectangle<int> rows[3];
    for (int i = 0; i < numRows; ++i)
    {
        rows[i] = area.removeFromTop (rowH);
        area.removeFromTop (gap);
    }

    if (DubBassProcessor::isSynth)
    {
        place (rows[0], { 0, 1, 2, 3 });
        place (rows[1], { 4, 5, 6 });
        place (rows[2], { 7, 8, 9 });
    }
    else
    {
        place (rows[0], { 0, 1, 2 });
        place (rows[1], { 3, 4, 5 });
    }
}

void IndikatPanel::timerCallback()
{
    envShown = 0.7f * envShown + 0.3f * juce::jlimit (0.0f, 1.0f, proc.envDisplay.load() * 2.0f);
    pumpShown = 1.0f - proc.pumpDisplay.load();
    const float db = juce::Decibels::gainToDecibels (proc.outLevel.load(), -60.0f);
    levelShown = 0.6f * levelShown + 0.4f * juce::jmap (db, -48.0f, 0.0f, 0.0f, 1.0f);

    repaint (meterArea.expanded (4));
    if (! pumpDot.isEmpty())
        repaint (pumpDot.expanded (12));

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
    {
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
        repaint();
    }
}

//==============================================================================
DubBassEditor::DubBassEditor (DubBassProcessor& p)
    : AudioProcessorEditor (&p), panel (p)
{
    addAndMakeVisible (panel);

    const int w = IndikatPanel::baseWidth(), h = IndikatPanel::baseHeight();
    setResizable (true, true);
    setResizeLimits (w * 6 / 10, h * 6 / 10, w * 2, h * 2);
    getConstrainer()->setFixedAspectRatio ((double) w / (double) h);
    setSize (w, h);
}

void DubBassEditor::resized()
{
    const float scale = (float) getWidth() / (float) IndikatPanel::baseWidth();
    panel.setBounds (0, 0, IndikatPanel::baseWidth(), IndikatPanel::baseHeight());
    panel.setTransform (juce::AffineTransform::scale (scale));
}
