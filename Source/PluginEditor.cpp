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
namespace
{
    float hashNote (int m)
    {
        const float s = std::sin ((float) m * 12.9898f) * 43758.5453f;
        return s - std::floor (s);
    }

    juce::String noteName (int m)
    {
        return juce::MidiMessage::getMidiNoteName (m, true, true, 4);
    }
}

//==============================================================================
StarField::StarField (StarPadProcessor& p) : proc (p)
{
    juce::Random r (0x57a7);
    for (int i = 0; i < 170; ++i)
    {
        const bool big = r.nextFloat() > 0.9f;
        stars.push_back ({ r.nextFloat(), r.nextFloat(), big ? 0.9f + r.nextFloat() * 0.9f : 0.35f + r.nextFloat() * 0.6f,
                           r.nextFloat() * 6.28f, 0.4f + r.nextFloat() * 2.2f, 0.15f + r.nextFloat() * 0.6f,
                           r.nextFloat() < 0.35f });
    }
    setOpaque (true);
}

void StarField::tick()
{
    const float target = juce::jmin (1.0f, proc.engine.level.load() * 1.6f);
    level += (target - level) * 0.18f;
    time += 1.0f / 30.0f;

    const float drift = 0.004f + level * 0.02f;
    for (auto& s : stars)
    {
        s.x -= drift / 30.0f * s.r * 0.6f;
        if (s.x < 0.0f) s.x += 1.0f;
    }

    // Neue Funken aus dem Audio-Thread
    auto& fifo = proc.engine.sparkFifo;
    int start1, size1, start2, size2;
    fifo.prepareToRead (fifo.getNumReady(), start1, size1, start2, size2);
    auto take = [this] (const StarPadEngine::Spark& sp)
    {
        flares.push_back ({ juce::jlimit (0.03f, 0.97f, 0.04f + 0.92f * ((sp.pitch - 60.0f) / 48.0f + rng.nextFloat() * 0.06f)),
                            0.1f + rng.nextFloat() * 0.48f, sp.level, 1.0f, sp.violet });
    };
    for (int i = 0; i < size1; ++i) take (proc.engine.sparkBuffer[start1 + i]);
    for (int i = 0; i < size2; ++i) take (proc.engine.sparkBuffer[start2 + i]);
    fifo.finishedRead (size1 + size2);

    for (auto& f : flares) f.life -= 0.026f;
    flares.erase (std::remove_if (flares.begin(), flares.end(), [] (const Flare& f) { return f.life <= 0.0f; }), flares.end());
    if (flares.size() > 60) flares.erase (flares.begin(), flares.begin() + (long) (flares.size() - 60));

    held.clear();
    const juce::uint64 m[2] { proc.engine.heldMask[0].load(), proc.engine.heldMask[1].load() };
    for (int n = 0; n < 128; ++n)
        if (m[n >> 6] & (1ull << (n & 63))) held.push_back (n);

    repaint();
}

void StarField::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth(), h = b.getHeight();

    g.fillAll (juce::Colour (0xff020409));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff14304f).withAlpha (0.35f + level * 0.5f), w * 0.5f, h * 1.15f,
                                             juce::Colour (0x00020409), w * 0.5f, h * 1.15f - w * 0.55f, true));
    g.fillRect (b);

    // Sterne
    for (auto& s : stars)
    {
        const float a = juce::jlimit (0.0f, 1.0f, s.bright * (0.55f + 0.45f * std::sin (s.phase + time * s.speed)) * (0.45f + level * 1.6f));
        g.setColour ((s.cool ? Palette::accent : juce::Colour (0xffd2dbe8)).withAlpha (a));
        g.fillRect (s.x * w, s.y * h, s.r, s.r);
    }

    // Horizont: Ausgangssignal
    if (level > 0.002f)
    {
        const int pos = proc.engine.scopePos.load();
        constexpr int span = 1024;
        juce::Path wave;
        for (int i = 0; i < span; i += 4)
        {
            const float v = proc.engine.scope[(pos - span + i) & (StarPadEngine::scopeSize - 1)];
            const float x = (float) i / (float) span * w;
            const float y = h * 0.64f + v * h * 0.28f;
            if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }
        strokeGlow (g, wave, Palette::accent.withAlpha (0.15f + level * 0.5f), 1.0f);
    }

    // Sternbild des gehaltenen Akkords
    if (! held.empty())
    {
        const int n = (int) held.size();
        std::vector<juce::Point<float>> pts;
        for (int i = 0; i < n; ++i)
        {
            const float fx = 0.22f + 0.56f * (n > 1 ? (float) i / (float) (n - 1) : 0.5f) + (hashNote (held[(size_t) i] + 3) - 0.5f) * 0.06f;
            pts.push_back ({ fx * w, (0.14f + 0.36f * hashNote (held[(size_t) i])) * h });
        }
        juce::Path line;
        line.startNewSubPath (pts[0]);
        for (size_t i = 1; i < pts.size(); ++i) line.lineTo (pts[i]);
        strokeGlow (g, line, Palette::accent.withAlpha (0.3f + level * 0.6f), 1.0f);
        for (auto& p : pts)
        {
            const float len = 5.0f + level * 16.0f;
            g.setColour (Palette::accent.withAlpha (0.5f + level * 0.5f));
            g.fillRect (p.x - len, p.y - 0.5f, len * 2.0f, 1.0f);
            g.fillRect (p.x - 0.5f, p.y - len, 1.0f, len * 2.0f);
            glowDot (g, p, 2.2f, juce::Colour (0xffe9fbff));
        }
    }

    // Funken
    for (auto& f : flares)
    {
        const auto col = f.violet ? Palette::counter : Palette::accent;
        const float x = f.x * w, y = f.y * h, len = f.size * f.life * 22.0f;
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.7f * f.life), x, y, col.withAlpha (0.0f), x + len * 0.8f, y, true));
        g.fillEllipse (juce::Rectangle<float> (len * 1.6f, len * 1.6f).withCentre ({ x, y }));
        g.setColour (juce::Colour (0xfff0fcff).withAlpha (f.life));
        g.fillRect (x - len, y - 0.5f, len * 2.0f, 1.0f);
        g.fillRect (x - 0.5f, y - len, 1.0f, len * 2.0f);
    }

    // Beschriftung
    auto inner = getLocalBounds().reduced (10, 6);
    g.setFont (fonts->mono (10.0f).withExtraKerningFactor (0.2f));
    g.setColour (Palette::dim);
    g.drawText ("STARFIELD.", inner.removeFromTop (14), juce::Justification::topLeft);
    g.drawText (juce::String (proc.hostBpm.load(), 1) + " BPM", getLocalBounds().reduced (10, 6), juce::Justification::topRight);
    g.setFont (fonts->mono (15.0f).withExtraKerningFactor (0.06f));
    g.setColour (chordText == "-" ? Palette::faint : Palette::accent);
    g.drawText (chordText, getLocalBounds().reduced (10, 6), juce::Justification::bottomLeft);
    g.setFont (fonts->mono (10.0f).withExtraKerningFactor (0.2f));
    g.setColour (Palette::dim);
    g.drawText ("STARS " + juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("stars")->load() * 100.0f)) + " %",
                getLocalBounds().reduced (10, 6), juce::Justification::bottomRight);

    g.setColour (Palette::hairHi);
    g.drawRect (getLocalBounds(), 1);
}

//==============================================================================
void ChordPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff0b1422));
    g.fillRoundedRectangle (r, 6.0f);

    if (lit)
    {
        g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (0.02f), r.getCentreX(), r.getY(),
                                                 Palette::accent.withAlpha (0.18f), r.getCentreX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Palette::accent.withAlpha (0.1f));
        g.drawRoundedRectangle (r.reduced (1.5f), 5.0f, 3.0f);
        g.setColour (Palette::accent.withAlpha (0.75f));
    }
    else
    {
        g.setColour (isMouseOver() ? Palette::hairHi : Palette::hair);
    }
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    auto inner = getLocalBounds().reduced (12, 9);
    g.setFont (fonts->mono (10.5f));
    g.setColour (lit ? Palette::accent : Palette::faint);
    g.drawText (juce::String (number), inner, juce::Justification::topLeft);
    g.setColour (Palette::dim);
    g.setFont (fonts->mono (10.5f).withExtraKerningFactor (0.2f));
    g.drawText (degree, inner, juce::Justification::bottomLeft);

    g.setFont (fonts->wide (14.0f).withExtraKerningFactor (0.04f));
    g.setColour (lit ? Palette::accent : Palette::text);
    g.drawFittedText (chord, inner.withTrimmedTop (4), juce::Justification::centredLeft, 1, 0.8f);
    if (lit)
        glowDot (g, { r.getRight() - 12.0f, r.getY() + 12.0f }, 2.2f, Palette::accent);
}

//==============================================================================
IndikatPanel::IndikatPanel (StarPadProcessor& p)
    : proc (p), starField (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    if (auto svg = juce::XmlDocument::parse (juce::String::fromUTF8 (BinaryData::indikatlogo_svg, BinaryData::indikatlogo_svgSize)))
    {
        logo = juce::Drawable::createFromSVG (*svg);
        if (logo != nullptr)
            logo->replaceColour (juce::Colours::white, Palette::text);
    }

    // 01 Akkorde (Pads statt Reglern)
    addChoice (key, keyAtt, "key");
    addChoice (chordSet, chordSetAtt, "chordSet");
    addChoice (voicing, voicingAtt, "voicing");
    addAndMakeVisible (chordOn);
    chordOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "chordOn", chordOn);
    addAndMakeVisible (hold);
    hold.onClick = [this] { if (! hold.getToggleState()) proc.padRequest.store (-1); };
    addSection ("CHORDS", {}, { { &hold, 74 }, { &chordOn, 116 }, { &voicing, 74 }, { &chordSet, 82 }, { &key, 92 } });

    for (int i = 0; i < StarPadEngine::numPads; ++i)
    {
        pads[i].number = i + 1;
        pads[i].onPress = [this, i] { padPressed (i); };
        pads[i].onRelease = [this, i] { padReleased (i); };
        addAndMakeVisible (pads[i]);
    }

    addChoice (wave, waveAtt, "wave");
    addSection ("SOUND", { &addKnob ("voices", "VOICES"), &addKnob ("detune", "DETUNE"), &addKnob ("octave", "OCTAVE"),
                           &addKnob ("sub", "SUB") },
                { { &wave, 96 } });

    addChoice (pulse, pulseAtt, "pulse");
    addSection ("ENVELOPE", { &addKnob ("attack", "ATTACK"), &addKnob ("release", "RELEASE"), &addKnob ("gate", "STAB") },
                { { &pulse, 108 } });

    addChoice (lfoRate, lfoRateAtt, "lfoRate");
    addSection ("FILTER", { &addKnob ("cutoff", "CUTOFF"), &addKnob ("reso", "RESO"), &addKnob ("lfoDepth", "LFO") },
                { { &lfoRate, 88 } });

    addChoice (delayDiv, delayDivAtt, "delayDiv");
    addSection ("SPACE", { &addKnob ("chorus", "CHORUS"), &addKnob ("feedback", "FEEDBACK"), &addKnob ("delayMix", "ECHO"),
                           &addKnob ("revSize", "SIZE"), &addKnob ("revMix", "REVERB") },
                { { &delayDiv, 76 } });

    addSection ("STARS", { &addKnob ("stars", "DENSITY"), &addKnob ("starHeight", "HEIGHT"), &addKnob ("starLevel", "LEVEL"),
                           &addKnob ("noise", "VINYL") });

    addSection ("OUT", { &addKnob ("outGain", "OUTPUT") });

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); repaint(); };
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

    addAndMakeVisible (starField);

    keyboard = std::make_unique<SpaceKeyboard> (proc.keyboardState);
    keyboard->setAvailableRange (36, 83);
    keyboard->setScrollButtonsVisible (false);
    keyboard->setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard->setBlackNoteLengthProportion (0.6f);
    keyboard->setOctaveForMiddleC (4);
    addAndMakeVisible (*keyboard);

    setSize (baseWidth(), baseHeight());
    timerCallback();
    startTimerHz (30);
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
    k->slider.sendLookAndFeelChange();
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

void IndikatPanel::padPressed (int i)
{
    if (hold.getToggleState() && proc.padRequest.load() == i)
        proc.padRequest.store (-1);
    else
        proc.padRequest.store (i);
}

void IndikatPanel::padReleased (int i)
{
    if (! hold.getToggleState() && proc.padRequest.load() == i)
        proc.padRequest.store (-1);
}

//==============================================================================
void IndikatPanel::renderBackground()
{
    const int w = getWidth(), h = getHeight();
    background = juce::Image (juce::Image::RGB, w, h, true);
    juce::Graphics g (background);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d1828), (float) w * 0.78f, (float) -h * 0.15f,
                                             Palette::space0, (float) w * 0.2f, (float) h * 1.1f, true));
    g.fillAll();

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

    const juce::Point<float> origin ((float) w * 1.02f, (float) -h * 0.25f);
    for (int i = 0; i < 4; ++i)
    {
        const float r = (float) h * (0.55f + 0.32f * (float) i);
        g.setColour (Palette::accent.withAlpha (0.035f - 0.006f * (float) i));
        g.drawEllipse (juce::Rectangle<float> (r * 2.4f, r * 2.0f).withCentre (origin), 1.0f);
    }
}

void IndikatPanel::paint (juce::Graphics& g)
{
    if (background.isNull() || background.getWidth() != getWidth() || background.getHeight() != getHeight())
        renderBackground();
    g.drawImageAt (background, 0, 0);

    if (logo != nullptr)
        logo->drawWithin (g, { 30.0f, 52.0f, 24.0f * 4.93f, 24.0f }, juce::RectanglePlacement::xLeft, 1.0f);
    glowDot (g, { 33.0f, 96.0f }, 2.2f, Palette::accent);
    g.setColour (Palette::dim);
    g.setFont (fonts->mono (11.5f).withExtraKerningFactor (0.45f));
    g.drawText ("STAR PAD", 44, 88, 200, 16, juce::Justification::centredLeft);
    g.setColour (Palette::faint);
    g.setFont (fonts->mono (10.0f).withExtraKerningFactor (0.2f));
    g.drawText ("PAD UNIT // 1.0", 31, 108, 140, 16, juce::Justification::centredLeft);

    // Preset-Nummer und Beschreibung
    g.setColour (Palette::dim);
    g.setFont (fonts->mono (10.0f).withExtraKerningFactor (0.35f));
    g.drawText ("PROGRAM " + juce::String (proc.getCurrentProgram() + 1).paddedLeft ('0', 2)
                    + " / " + juce::String (proc.getNumPrograms()).paddedLeft ('0', 2),
                presetBox.getBounds().translated (0, -22).withHeight (16), juce::Justification::centred);
    g.setColour (Palette::faint);
    g.setFont (fonts->label (11.5f));
    g.drawFittedText (proc.getProgramDescription (proc.getCurrentProgram()),
                      presetBox.getBounds().translated (0, 34).withHeight (30).expanded (24, 0),
                      juce::Justification::centredTop, 2);

    juce::ColourGradient line (Palette::hairHi.withAlpha (0.0f), 28.0f, 0.0f,
                               Palette::hairHi.withAlpha (0.0f), (float) getWidth() - 28.0f, 0.0f, false);
    line.addColour (0.15, Palette::hairHi);
    line.addColour (0.85, Palette::hairHi);
    g.setGradientFill (line);
    g.fillRect (28, 158, getWidth() - 56, 1);

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

    g.setColour (Palette::hair);
    g.drawRect (keyboard->getBounds().expanded (1), 1);
}

juce::Rectangle<int> IndikatPanel::layoutSection (Section& s, juce::Rectangle<int> r)
{
    s.bounds = r;
    auto inner = r.reduced (12, 10);
    auto header = inner.removeFromTop (20);

    for (auto& [comp, width] : s.headerControls)
    {
        comp->setBounds (header.removeFromRight (width));
        header.removeFromRight (10);
    }

    inner.removeFromTop (8);
    if (! s.knobs.empty())
    {
        const int knobW = inner.getWidth() / (int) s.knobs.size();
        for (auto* k : s.knobs)
        {
            auto cell = inner.removeFromLeft (knobW);
            k->label.setBounds (cell.removeFromTop (16));
            k->slider.setBounds (cell.reduced (2, 0));
        }
    }
    return inner;
}

void IndikatPanel::place (juce::Rectangle<int> row, std::initializer_list<int> idx)
{
    constexpr int gap = 12;
    auto weightOf = [this] (int i) { return juce::jmax (1.3f, (float) sections[(size_t) i].knobs.size()); };
    float totalWeight = 0.0f;
    for (int i : idx) totalWeight += weightOf (i);
    const int avail = row.getWidth() - gap * ((int) idx.size() - 1);

    int n = 0;
    for (int i : idx)
    {
        auto& s = sections[(size_t) i];
        const int w = ++n == (int) idx.size() ? row.getWidth() : juce::roundToInt ((float) avail * weightOf (i) / totalWeight);
        layoutSection (s, row.removeFromLeft (w));
        row.removeFromLeft (gap);
    }
}

void IndikatPanel::resized()
{
    auto area = getLocalBounds().reduced (28, 0);

    // Header: Marke | Sternenfeld | Presets
    auto header = juce::Rectangle<int> (area.getX(), 20, area.getWidth(), 126);
    auto presetColumn = header.removeFromRight (270);
    auto presetArea = presetColumn.withTrimmedTop (44).withHeight (28);
    nextPreset.setBounds (presetArea.removeFromRight (28));
    prevPreset.setBounds (presetArea.removeFromLeft (28));
    presetBox.setBounds (presetArea.reduced (8, 0));
    header.removeFromRight (30);
    starField.setBounds (header.removeFromRight (520));

    // Sektionen
    area.setTop (172);
    area.removeFromBottom (24);
    keyboard->setBounds (area.removeFromBottom (70));
    keyboard->setKeyWidth ((float) keyboard->getWidth() / 28.0f);
    area.removeFromBottom (14);

    constexpr int gap = 12;
    auto padArea = layoutSection (sections[0], area.removeFromTop (142));
    area.removeFromTop (gap);
    {
        const int padGap = 10;
        const int pw = (padArea.getWidth() - padGap * (StarPadEngine::numPads - 1)) / StarPadEngine::numPads;
        for (int i = 0; i < StarPadEngine::numPads; ++i)
        {
            pads[i].setBounds (padArea.removeFromLeft (pw).withTrimmedTop (2));
            padArea.removeFromLeft (padGap);
        }
    }

    const int rowH = (area.getHeight() - gap) / 2;
    place (area.removeFromTop (rowH), { 1, 2, 3 });
    area.removeFromTop (gap);
    place (area, { 4, 5, 6 });

    int knobWidth = std::numeric_limits<int>::max();
    for (auto& k : knobs)
        knobWidth = juce::jmin (knobWidth, k->slider.getWidth());
    for (auto& k : knobs)
        k->slider.setBounds (k->slider.getBounds().withSizeKeepingCentre (knobWidth, k->slider.getHeight()));
}

void IndikatPanel::timerCallback()
{
    const auto params = proc.readParams();
    const int pad = proc.padRequest.load();

    if (params.chordSet != lastSet || params.key != lastKey || pad != lastPad)
    {
        for (int i = 0; i < StarPadEngine::numPads; ++i)
        {
            pads[i].chord = StarPadEngine::padLabel (params.chordSet, params.key, i);
            pads[i].degree = StarPadEngine::padDegree (params.chordSet, i);
            pads[i].lit = pad == i;
            pads[i].repaint();
        }
        lastSet = params.chordSet; lastKey = params.key; lastPad = pad;
    }

    if (pad >= 0)
        starField.chordText = StarPadEngine::padLabel (params.chordSet, params.key, pad);
    else
    {
        juce::StringArray names;
        const juce::uint64 m[2] { proc.engine.heldMask[0].load(), proc.engine.heldMask[1].load() };
        for (int n = 0; n < 128 && names.size() < 5; ++n)
            if (m[n >> 6] & (1ull << (n & 63))) names.add (noteName (n));
        starField.chordText = names.isEmpty() ? "-" : names.joinIntoString (" ");
    }
    starField.tick();

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
    {
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
        repaint();
    }
}

//==============================================================================
StarPadEditor::StarPadEditor (StarPadProcessor& p)
    : AudioProcessorEditor (&p), panel (p)
{
    addAndMakeVisible (panel);

    const int w = IndikatPanel::baseWidth(), h = IndikatPanel::baseHeight();
    setResizable (true, true);
    setResizeLimits (w * 6 / 10, h * 6 / 10, w * 2, h * 2);
    getConstrainer()->setFixedAspectRatio ((double) w / (double) h);
    setSize (w, h);
}

void StarPadEditor::resized()
{
    const float scale = (float) getWidth() / (float) IndikatPanel::baseWidth();
    panel.setBounds (0, 0, IndikatPanel::baseWidth(), IndikatPanel::baseHeight());
    panel.setTransform (juce::AffineTransform::scale (scale));
}
