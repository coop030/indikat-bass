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
    const juce::Colour counter { 0xffa292ff };   // Glitch-Violett
    const juce::Colour star    { 0xffe8eef8 };

    // Pluto
    const juce::Colour ice     { 0xffc9b6a1 };
    const juce::Colour dust    { 0xff9a8270 };
    const juce::Colour tholin  { 0xff4b2f28 };   // dunkle Äquatorzone
    const juce::Colour heart   { 0xfff2ece4 };   // Tombaugh Regio
}

namespace
{
    void strokeGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width)
    {
        g.setColour (c.withAlpha (c.getFloatAlpha() * 0.06f));
        g.strokePath (p, juce::PathStrokeType (width * 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (c.getFloatAlpha() * 0.14f));
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

    juce::Colour elementColour (int e)
    {
        if (e == PlutoEngine::Glitch) return Palette::counter;
        if (e >= PlutoEngine::numBeds) return Palette::star;
        return Palette::accent;
    }

    // Oberflächenflecken auf der Kugel: Breite, Länge, Winkelradius, Farbe
    struct Feature { float lat, lon, size; juce::Colour colour; };

    const std::vector<Feature>& plutoSurface()
    {
        static const std::vector<Feature> features = []
        {
            std::vector<Feature> f;
            juce::Random r (0x504c544f);   // "PLTO"

            for (int i = 0; i < 70; ++i)   // Marmorierung
                f.push_back ({ std::asin (r.nextFloat() * 1.9f - 0.95f), r.nextFloat() * juce::MathConstants<float>::twoPi,
                               0.08f + 0.22f * r.nextFloat(),
                               (r.nextBool() ? Palette::dust : Palette::ice.brighter (0.1f)).withAlpha (0.25f + 0.3f * r.nextFloat()) });

            for (int i = 0; i < 46; ++i)   // dunkler Gürtel westlich des Herzens
                f.push_back ({ -0.25f + 0.4f * r.nextFloat(), -2.7f + 2.0f * r.nextFloat(), 0.12f + 0.16f * r.nextFloat(),
                               Palette::tholin.withAlpha (0.55f + 0.35f * r.nextFloat()) });

            for (float y = -1.1f; y <= 1.3f; y += 0.16f)   // das Herz
                for (float x = -1.3f; x <= 1.3f; x += 0.16f)
                {
                    const float a = x * x + y * y - 1.0f;
                    if (a * a * a - x * x * y * y * y <= 0.0f)
                        f.push_back ({ 0.18f + y * 0.42f, 0.35f + x * 0.5f, 0.11f + 0.04f * r.nextFloat(),
                                       Palette::heart.withAlpha (0.55f + 0.3f * r.nextFloat()) });
                }

            for (int i = 0; i < 18; ++i)   // Krater
                f.push_back ({ std::asin (r.nextFloat() * 1.8f - 0.9f), r.nextFloat() * juce::MathConstants<float>::twoPi,
                               0.03f + 0.05f * r.nextFloat(), juce::Colours::black.withAlpha (0.25f) });
            return f;
        }();
        return features;
    }

    void drawPluto (juce::Graphics& g, juce::Point<float> c, float R, double spin, float glow)
    {
        const auto disc = juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c);

        // Blauer Dunst der dünnen Atmosphäre
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (Palette::accent.withAlpha ((0.012f + 0.04f * glow) * (float) (4 - i)));
            g.fillEllipse (disc.expanded (R * 0.05f * (float) i));
        }

        juce::ColourGradient body (Palette::ice.brighter (0.2f), c.x - R * 0.5f, c.y - R * 0.55f,
                                   Palette::dust.darker (0.6f), c.x + R * 0.8f, c.y + R * 0.8f, true);
        g.setGradientFill (body);
        g.fillEllipse (disc);

        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addEllipse (disc);
            g.reduceClipRegion (clip);

            for (auto& f : plutoSurface())
            {
                const float lon = f.lon + (float) spin;
                const float depth = std::cos (lon) * std::cos (f.lat);
                if (depth <= 0.0f)
                    continue;
                const auto p = juce::Point<float> (c.x + R * std::sin (lon) * std::cos (f.lat), c.y - R * std::sin (f.lat));
                const float s = R * std::sin (f.size);
                g.setColour (f.colour.withMultipliedAlpha (juce::jmin (1.0f, depth * 2.5f)));
                g.fillEllipse (juce::Rectangle<float> (s * 2.0f * std::cos (lon), s * 2.0f * std::cos (f.lat)).withCentre (p));
            }

            // Nachtseite
            juce::ColourGradient night (juce::Colours::transparentBlack, c.x - R * 0.25f, c.y - R * 0.3f,
                                        Palette::space0.withAlpha (0.95f), c.x + R * 0.85f, c.y + R * 0.8f, false);
            g.setGradientFill (night);
            g.fillEllipse (disc);
        }

        // Kaltes Randlicht auf der Tagseite
        juce::Path rim;
        rim.addCentredArc (c.x, c.y, R - 0.8f, R - 0.8f, 0.0f, -2.4f, 0.3f, true);
        strokeGlow (g, rim, Palette::accent.withAlpha (0.45f + 0.4f * glow), 1.2f);
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
PlanetButton::PlanetButton()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Randomize");
    setDescription ("Neue zufällige Atmo-Szene");
}

bool PlanetButton::hitTest (int x, int y)
{
    return getLocalBounds().toFloat().getCentre().getDistanceFrom ({ (float) x, (float) y }) <= radius() + 6.0f;
}

void PlanetButton::mouseDown (const juce::MouseEvent&)
{
    pressed = true;
    repaint();
}

void PlanetButton::mouseUp (const juce::MouseEvent& e)
{
    pressed = false;
    repaint();
    if (hitTest (e.x, e.y) && onClick != nullptr)
        onClick();
}

void PlanetButton::paint (juce::Graphics& g)
{
    const float glow = pressed ? 1.0f : (isMouseOver() ? 0.6f : 0.0f);
    drawPluto (g, getLocalBounds().toFloat().getCentre(), radius() * (pressed ? 0.97f : 1.0f), spin, glow);
}

//==============================================================================
IndikatPanel::IndikatPanel (PlutoProcessor& p)
    : proc (p)
{
    setOpaque (true);

    if (auto svg = juce::XmlDocument::parse (juce::String::fromUTF8 (BinaryData::indikatlogo_svg, BinaryData::indikatlogo_svgSize)))
    {
        logo = juce::Drawable::createFromSVG (*svg);
        if (logo != nullptr)
            logo->replaceColour (juce::Colours::white, Palette::text);
    }

    planet.onClick = [this]
    {
        proc.randomize();
        clickTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
        spinBoost = 4.0f;
    };
    addAndMakeVisible (planet);

    lastEventCount = proc.engine.display.eventCount.load();
    lastTick = juce::Time::getMillisecondCounterHiRes() * 0.001;

    setSize (baseWidth(), baseHeight());
    startTimerHz (30);
}

//==============================================================================
void IndikatPanel::renderBackground()
{
    const int w = getWidth(), h = getHeight();
    background = juce::Image (juce::Image::RGB, w, h, true);
    juce::Graphics g (background);

    // Tiefer Raum; die Sonne ist hier draußen nur noch ein heller Stern
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d1828), (float) w * 0.8f, (float) -h * 0.1f,
                                             Palette::space0, (float) w * 0.2f, (float) h * 1.0f, true));
    g.fillAll();

    juce::Random stars (0x1d1ca7);
    for (int i = 0; i < 300; ++i)
    {
        const float x = stars.nextFloat() * (float) w, y = stars.nextFloat() * (float) h;
        const float bright = std::pow (stars.nextFloat(), 3.0f);
        const float r = 0.35f + bright * 0.9f;
        const auto col = stars.nextFloat() < 0.15f ? Palette::accent : Palette::star;
        g.setColour (col.withAlpha (0.08f + bright * 0.55f));
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
    }
    glowDot (g, { (float) w - 70.0f, 150.0f }, 1.6f, Palette::star);   // die ferne Sonne

    // Umlaufbahnen
    for (int i = 0; i < 2; ++i)
    {
        const float r = 172.0f + 30.0f * (float) i;
        g.setColour (Palette::accent.withAlpha (0.07f - 0.025f * (float) i));
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f * 0.92f).withCentre (centre), 1.0f);
    }
}

void IndikatPanel::drawSparks (juce::Graphics& g, double now) const
{
    for (auto& s : sparks)
    {
        const float age = (float) (now - s.born);
        const bool glitch = s.type == PlutoEngine::Glitch;
        const float life = glitch ? 0.6f : 1.8f;
        if (age >= life) continue;
        const float k = age / life;
        const auto col = elementColour (s.type);

        if (glitch)
        {
            juce::Random r ((juce::int64) (s.born * 1000.0) + (juce::int64) (age * 20.0f));
            for (int i = 0; i < 4; ++i)
            {
                const float wdt = 12.0f + 50.0f * r.nextFloat();
                g.setColour ((i % 2 == 0 ? col : Palette::accent).withAlpha (0.5f * (1.0f - k)));
                g.fillRect (s.pos.x - wdt * 0.5f + (r.nextFloat() - 0.5f) * 30.0f,
                            s.pos.y + (r.nextFloat() - 0.5f) * 22.0f, wdt, 1.5f);
            }
            glowDot (g, s.pos, 2.0f, col.withAlpha (1.0f - k));
            continue;
        }

        const float big = (s.type == PlutoEngine::Pings || s.type == PlutoEngine::Sweeps) ? 1.0f : 0.6f;
        const float ringR = 3.0f + 26.0f * big * std::sqrt (k);
        g.setColour (col.withAlpha (0.45f * (1.0f - k)));
        g.drawEllipse (juce::Rectangle<float> (ringR * 2.0f, ringR * 2.0f).withCentre (s.pos), 1.0f);
        glowDot (g, s.pos, 2.4f * (1.0f - 0.5f * k), col.withAlpha (1.0f - k));
    }
}

void IndikatPanel::drawChip (juce::Graphics& g, juce::Rectangle<int> r, int e) const
{
    const bool on = (activeMask >> e) & 1u;
    const float act = activity[(size_t) e];
    const auto col = elementColour (e);
    auto rf = r.toFloat().reduced (3.0f, 3.0f);
    const float rad = rf.getHeight() * 0.5f;

    g.setColour (on ? col.withAlpha (0.05f + 0.08f * act) : juce::Colours::transparentBlack);
    g.fillRoundedRectangle (rf, rad);
    g.setColour (on ? col.withAlpha (0.35f + 0.4f * act) : Palette::hair);
    g.drawRoundedRectangle (rf, rad, 1.0f);

    const juce::Point<float> dot (rf.getX() + rad + 2.0f, rf.getCentreY());
    if (on) glowDot (g, dot, 1.6f + 1.2f * act, col.withAlpha (0.5f + 0.5f * act));
    else    { g.setColour (Palette::faint); g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (dot)); }

    g.setColour (on ? Palette::text : Palette::faint);
    g.setFont (fonts->mono (10.5f).withExtraKerningFactor (0.14f));
    g.drawText (PlutoEngine::elementName (e), rf.withTrimmedLeft (rad * 2.0f + 6.0f), juce::Justification::centredLeft);
}

void IndikatPanel::paint (juce::Graphics& g)
{
    if (background.isNull() || background.getWidth() != getWidth() || background.getHeight() != getHeight())
        renderBackground();
    g.drawImageAt (background, 0, 0);

    // Logo (Vektor, 4.93 : 1)
    if (logo != nullptr)
        logo->drawWithin (g, { 30.0f, 32.0f, 22.0f * 4.93f, 22.0f }, juce::RectanglePlacement::xLeft, 1.0f);
    glowDot (g, { 33.0f, 76.0f }, 2.2f, Palette::accent);
    g.setColour (Palette::dim);
    g.setFont (fonts->mono (11.5f).withExtraKerningFactor (0.45f));
    g.drawText ("PLUTO", 44, 68, 200, 16, juce::Justification::centredLeft);
    g.setColour (Palette::faint);
    g.setFont (fonts->mono (10.0f).withExtraKerningFactor (0.2f));
    g.drawText ("ATMO UNIT // 2.0", 31, 86, 160, 16, juce::Justification::centredLeft);

    // Tempo vom Host
    const int right = getWidth() - 30;
    g.setColour (Palette::text);
    g.setFont (fonts->mono (22.0f).withExtraKerningFactor (0.08f));
    g.drawText (juce::String (proc.bpmShown.load(), 1), right - 160, 36, 160, 26, juce::Justification::centredRight);
    g.setColour (Palette::faint);
    g.setFont (fonts->mono (10.0f).withExtraKerningFactor (0.3f));
    g.drawText ("BPM // GRID 1/16", right - 200, 68, 200, 16, juce::Justification::centredRight);

    // Trennlinie unter dem Header
    juce::ColourGradient line (Palette::hairHi.withAlpha (0.0f), 28.0f, 0.0f,
                               Palette::hairHi.withAlpha (0.0f), (float) getWidth() - 28.0f, 0.0f, false);
    line.addColour (0.15, Palette::hairHi);
    line.addColour (0.85, Palette::hairHi);
    g.setGradientFill (line);
    g.fillRect (28, 116, getWidth() - 56, 1);

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    // Schockwelle nach dem Klick
    const float sinceClick = (float) (now - clickTime);
    if (sinceClick < 1.2f)
    {
        const float k = sinceClick / 1.2f;
        const float r = planet.radius() + (210.0f - planet.radius()) * (1.0f - std::pow (1.0f - k, 3.0f));
        juce::Path wave;
        wave.addEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f * 0.92f).withCentre (centre));
        strokeGlow (g, wave, Palette::accent.withAlpha (0.8f * (1.0f - k)), 1.4f);
    }

    // Charon auf seiner Bahn
    const float charonA = (float) std::fmod (planet.spin * 0.45, juce::MathConstants<double>::twoPi);
    auto charon = centre.getPointOnCircumference (172.0f, charonA);
    charon.y = centre.y + (charon.y - centre.y) * 0.92f;
    glowDot (g, charon, 2.6f, Palette::ice.withAlpha (0.9f));

    drawSparks (g, now);

    // Szene und Hinweis
    g.setFont (fonts->wide (9.0f).withExtraKerningFactor (0.45f));
    g.setColour (Palette::text.withAlpha (0.85f));
    g.drawText ("TAP PLUTO", 0, 128, getWidth(), 18, juce::Justification::centred);
    g.setFont (fonts->mono (12.0f).withExtraKerningFactor (0.3f));
    g.setColour (Palette::dim);
    g.drawText ("SCENE " + juce::String::toHexString ((int) shownSeed).toUpperCase().paddedLeft ('0', 8),
                0, layerBox.getY() - 34, getWidth(), 18, juce::Justification::centred);

    // Elemente
    auto box = layerBox.toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.012f));
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (Palette::hair);
    g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);
    g.setFont (fonts->mono (10.5f));
    g.setColour (Palette::accent.withAlpha (0.8f));
    g.drawText ("01", layerBox.getX() + 14, layerBox.getY() + 10, 24, 18, juce::Justification::centredLeft);
    g.setFont (fonts->wide (9.0f).withExtraKerningFactor (0.3f));
    g.setColour (Palette::text.withAlpha (0.85f));
    g.drawText ("LAYERS", layerBox.getX() + 36, layerBox.getY() + 10, 200, 18, juce::Justification::centredLeft);

    auto grid = layerBox.reduced (12, 0).withTrimmedTop (36).withTrimmedBottom (10);
    const int rowH = grid.getHeight() / 3, colW = grid.getWidth() / 4;
    for (int e = 0; e < PlutoEngine::numElements; ++e)
        drawChip (g, { grid.getX() + (e % 4) * colW, grid.getY() + (e / 4) * rowH, colW, rowH },
                  e);
}

void IndikatPanel::resized()
{
    centre = { (float) getWidth() * 0.5f, 336.0f };
    planet.setBounds (juce::Rectangle<int> (284, 284).withCentre (centre.roundToInt()));
    layerBox = juce::Rectangle<int> (28, getHeight() - 28 - 128, getWidth() - 56, 128);
    background = {};
}

void IndikatPanel::timerCallback()
{
    auto& d = proc.engine.display;
    for (int e = 0; e < PlutoEngine::numElements; ++e)
        activity[(size_t) e] = 0.6f * activity[(size_t) e] + 0.4f * d.activity[(size_t) e].load();
    activeMask = d.activeMask.load();
    shownSeed = proc.getSeed();

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const double dt = juce::jlimit (0.0, 0.2, now - lastTick);
    lastTick = now;
    spinBoost *= (float) std::exp (-dt * 2.5);
    planet.spin += dt * (0.12 + spinBoost);

    // Neue Ereignisse als Funken auf der Umlaufbahn
    const int count = d.eventCount.load (std::memory_order_acquire);
    if (count - lastEventCount > PlutoEngine::Display::ringSize)
        lastEventCount = count - PlutoEngine::Display::ringSize;
    for (; lastEventCount < count; ++lastEventCount)
    {
        const auto idx = (size_t) (lastEventCount % PlutoEngine::Display::ringSize);
        const float pan = d.eventPan[idx].load (std::memory_order_relaxed);
        float a = (pan - 0.5f) * juce::MathConstants<float>::pi;
        if (rng.nextBool()) a = juce::MathConstants<float>::pi - a;
        auto pos = centre.getPointOnCircumference (172.0f + 30.0f * (float) rng.nextInt (2), a);
        pos.y = centre.y + (pos.y - centre.y) * 0.92f;
        sparks.push_back ({ pos, d.eventType[idx].load (std::memory_order_relaxed), now });
    }
    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [now] (const Spark& s) { return now - s.born > 2.0; }),
                  sparks.end());

    repaint();
}

//==============================================================================
PlutoEditor::PlutoEditor (PlutoProcessor& p)
    : AudioProcessorEditor (&p), panel (p)
{
    addAndMakeVisible (panel);

    const int w = IndikatPanel::baseWidth(), h = IndikatPanel::baseHeight();
    setResizable (true, true);
    setResizeLimits (w * 6 / 10, h * 6 / 10, w * 2, h * 2);
    getConstrainer()->setFixedAspectRatio ((double) w / (double) h);
    setSize (w, h);
}

void PlutoEditor::resized()
{
    const float scale = (float) getWidth() / (float) IndikatPanel::baseWidth();
    panel.setBounds (0, 0, IndikatPanel::baseWidth(), IndikatPanel::baseHeight());
    panel.setTransform (juce::AffineTransform::scale (scale));
}
