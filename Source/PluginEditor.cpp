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

    //==========================================================================
    // Plutos Oberfläche als Textur (Länge × Breite), einmal prozedural erzeugt
    struct V3
    {
        V3 (float x_, float y_, float z_) : x (x_), y (y_), z (z_) {}
        float x, y, z;
        float operator* (const V3& o) const { return x * o.x + y * o.y + z * o.z; }   // Skalarprodukt
        V3 normalised() const { const float l = std::sqrt (x * x + y * y + z * z); return { x / l, y / l, z / l }; }
    };

    float hash3 (int x, int y, int z)
    {
        auto h = (juce::uint32) x * 374761393u + (juce::uint32) y * 668265263u + (juce::uint32) z * 1274126177u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xffffff) / 16777216.0f;
    }

    float valueNoise (float x, float y, float z)
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y), zi = (int) std::floor (z);
        auto fade = [] (float t) { return t * t * (3.0f - 2.0f * t); };
        const float fx = fade (x - (float) xi), fy = fade (y - (float) yi), fz = fade (z - (float) zi);
        auto lerp = [] (float a, float b, float t) { return a + (b - a) * t; };
        const float x00 = lerp (hash3 (xi, yi, zi),         hash3 (xi + 1, yi, zi),         fx);
        const float x10 = lerp (hash3 (xi, yi + 1, zi),     hash3 (xi + 1, yi + 1, zi),     fx);
        const float x01 = lerp (hash3 (xi, yi, zi + 1),     hash3 (xi + 1, yi, zi + 1),     fx);
        const float x11 = lerp (hash3 (xi, yi + 1, zi + 1), hash3 (xi + 1, yi + 1, zi + 1), fx);
        return lerp (lerp (x00, x10, fy), lerp (x01, x11, fy), fz);
    }

    float fbm (V3 p, float freq, int octaves, float offset = 0.0f)
    {
        float sum = 0.0f, amp = 0.5f, norm = 0.0f;
        for (int o = 0; o < octaves; ++o)
        {
            sum += amp * valueNoise (p.x * freq + offset, p.y * freq - offset, p.z * freq + 2.0f * offset);
            norm += amp;
            amp *= 0.5f;
            freq *= 2.03f;
        }
        return sum / norm;
    }

    float wrapAngle (float a)
    {
        while (a > juce::MathConstants<float>::pi)  a -= juce::MathConstants<float>::twoPi;
        while (a < -juce::MathConstants<float>::pi) a += juce::MathConstants<float>::twoPi;
        return a;
    }

    float smooth (float e0, float e1, float x)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (x - e0) / (e1 - e0));
        return t * t * (3.0f - 2.0f * t);
    }

    struct Rgb { float r, g, b; };
    Rgb mix (Rgb a, Rgb b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }
    Rgb rgb (juce::Colour c) { return { c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue() }; }

    struct PlutoTexture
    {
        static constexpr int W = 768, H = 384;
        std::vector<Rgb> albedo;
        std::vector<float> gLon, gLat;   // Relief: Steigung in Längen- und Breitenrichtung

        PlutoTexture()
        {
            albedo.resize ((size_t) (W * H));
            std::vector<float> height ((size_t) (W * H));

            const Rgb tan     = rgb (juce::Colour (0xffb5977c));
            const Rgb beige   = rgb (juce::Colour (0xffe2cfb7));
            const Rgb pole    = rgb (juce::Colour (0xff8e8379));
            const Rgb tholin  = rgb (juce::Colour (0xff3b2420));
            const Rgb tholin2 = rgb (juce::Colour (0xff6a4031));
            const Rgb sputnik = rgb (juce::Colour (0xffe8e2d9));
            const Rgb heartE  = rgb (juce::Colour (0xffd9ccbb));

            // Krater (fester Zufall)
            struct Crater { V3 c; float r; };
            std::vector<Crater> craters;
            juce::Random cr (0x504c544f);
            for (int i = 0; i < 45; ++i)
            {
                const float lat = std::asin (cr.nextFloat() * 1.9f - 0.95f), lon = cr.nextFloat() * juce::MathConstants<float>::twoPi;
                craters.push_back ({ { std::cos (lat) * std::sin (lon), std::sin (lat), std::cos (lat) * std::cos (lon) },
                                     0.012f + 0.06f * std::pow (cr.nextFloat(), 2.0f) });
            }

            for (int j = 0; j < H; ++j)
            {
                const float lat = juce::MathConstants<float>::halfPi - ((float) j + 0.5f) / (float) H * juce::MathConstants<float>::pi;
                for (int i = 0; i < W; ++i)
                {
                    const float lon = ((float) i + 0.5f) / (float) W * juce::MathConstants<float>::twoPi - juce::MathConstants<float>::pi;
                    const V3 p (std::cos (lat) * std::sin (lon), std::sin (lat), std::cos (lat) * std::cos (lon));

                    const float n1 = fbm (p, 2.5f, 5), n2 = fbm (p, 9.0f, 4, 7.0f), n3 = fbm (p, 5.0f, 4, 3.0f);
                    const float n4 = fbm (p, 14.0f, 3, 5.0f);   // ausgefranste Ränder
                    Rgb c = mix (tan, beige, smooth (0.3f, 0.7f, n1));
                    const float grain = 0.88f + 0.24f * n2;

                    c = mix (c, pole, smooth (0.8f, 1.3f, lat) * 0.65f);   // dunklere Nordpolregion

                    // Dunkler Äquatorgürtel (Cthulhu) westlich des Herzens, dazu kleinere Flecken östlich
                    const float dl = wrapAngle (lon + 1.25f), db = lat + 0.12f;
                    const float e1 = (dl / 1.25f) * (dl / 1.25f) + (db / 0.3f) * (db / 0.3f) + (n3 - 0.5f) * 1.8f + (n4 - 0.5f) * 0.7f;
                    const float dl2 = wrapAngle (lon - 2.25f), db2 = lat + 0.16f;
                    const float e2 = (dl2 / 0.5f) * (dl2 / 0.5f) + (db2 / 0.2f) * (db2 / 0.2f) + (n3 - 0.5f) * 1.8f + (n4 - 0.5f) * 0.7f;
                    const float dark = juce::jmax (1.0f - smooth (0.55f, 1.1f, e1), 0.8f * (1.0f - smooth (0.5f, 1.05f, e2)))
                                     * (0.7f + 0.35f * n3);
                    c = mix (c, mix (tholin, tholin2, n2), dark);

                    // Das Herz (Tombaugh Regio): links die glatte Eisebene Sputnik Planitia
                    const float u = wrapAngle (lon - 0.42f) * std::cos (lat) / 0.55f, v = (lat + 0.02f) / 0.5f;
                    const float a = u * u + v * v - 1.0f;
                    const float f = a * a * a - u * u * v * v * v + (fbm (p, 6.0f, 3, 11.0f) - 0.5f) * 0.35f;
                    const float heart = 1.0f - smooth (-0.06f, 0.08f, f);
                    const float west = smooth (0.15f, -0.25f, u);
                    c = mix (c, mix (heartE, sputnik, west), heart * 0.95f);

                    float h = 0.55f * n2 + 0.45f * n1;
                    h *= 1.0f - heart * west * 0.85f;   // Sputnik ist flach

                    // Krater: dunkle Mulde, heller Rand (nicht in der Eisebene)
                    float shade = 1.0f;
                    for (auto& k : craters)
                    {
                        const float d = std::acos (juce::jlimit (-1.0f, 1.0f, p * k.c));
                        if (d > k.r * 1.4f) continue;
                        const float q = d / k.r;
                        const float amount = 1.0f - heart * west;
                        if (q < 1.0f) { shade *= 1.0f - 0.1f * amount * (1.0f - q * q); h -= 0.18f * amount * (1.0f - q * q); }
                        else          { const float rim = 1.0f - (q - 1.0f) / 0.4f; shade *= 1.0f + 0.05f * amount * rim; h += 0.08f * amount * rim; }
                    }

                    const float g = grain * shade;
                    albedo[(size_t) (j * W + i)] = { c.r * g, c.g * g, c.b * g };
                    height[(size_t) (j * W + i)] = h;
                }
            }

            gLon.resize (albedo.size());
            gLat.resize (albedo.size());
            const float dLon = juce::MathConstants<float>::twoPi / (float) W, dLat = juce::MathConstants<float>::pi / (float) H;
            for (int j = 0; j < H; ++j)
            {
                const float lat = juce::MathConstants<float>::halfPi - ((float) j + 0.5f) / (float) H * juce::MathConstants<float>::pi;
                const float cosLat = juce::jmax (0.15f, std::cos (lat));
                for (int i = 0; i < W; ++i)
                {
                    const auto at = [&] (int ii, int jj) { return height[(size_t) (juce::jlimit (0, H - 1, jj) * W + ((ii + W) % W))]; };
                    gLon[(size_t) (j * W + i)] = (at (i + 1, j) - at (i - 1, j)) / (2.0f * dLon * cosLat);
                    gLat[(size_t) (j * W + i)] = (at (i, j - 1) - at (i, j + 1)) / (2.0f * dLat);
                }
            }
        }
    };

    const PlutoTexture& plutoTexture()
    {
        static const PlutoTexture texture;
        return texture;
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
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::PopupMenu::backgroundColourId, Palette::space1);
    setColour (juce::PopupMenu::textColourId, Palette::text);
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

juce::Font IndikatLookAndFeel::getComboBoxFont (juce::ComboBox&) { return fonts->mono (13.0f).withExtraKerningFactor (0.12f); }

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

void PlanetButton::prepareSphere (int size)
{
    // Pro Pixel: Texturkoordinate (vor der Drehung) und feste Lichtanteile.
    // Die Drehung um die Polachse verschiebt danach nur noch die Länge.
    sphereSize = size;
    sphere.assign ((size_t) (size * size), {});
    plutoTexture();   // Textur schon hier erzeugen, nicht erst beim ersten Zeichnen

    const float tilt = 0.32f;
    const float ct = std::cos (tilt), st = std::sin (tilt);
    V3 light (-0.55f, 0.45f, 0.70f);
    light = light.normalised();
    const V3 L1 (light.x, light.y * ct - light.z * st, light.y * st + light.z * ct);
    const float discR = (float) size / (2.0f * haloScale);

    for (int py = 0; py < size; ++py)
        for (int px = 0; px < size; ++px)
        {
            auto& sp = sphere[(size_t) (py * size + px)];
            const float x = ((float) px + 0.5f - (float) size * 0.5f) / discR;
            const float y = ((float) size * 0.5f - (float) py - 0.5f) / discR;
            const float r = std::sqrt (x * x + y * y);

            // Dunstschleier außerhalb der Scheibe, auf der Lichtseite heller
            if (r > 0.985f && r < haloScale)
            {
                const float light2D = juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * (x * light.x + y * light.y) / juce::jmax (0.001f, r)
                                                                    / std::sqrt (light.x * light.x + light.y * light.y));
                const float t = juce::jlimit (0.0f, 1.0f, (r - 1.0f) / (haloScale - 1.0f));
                sp.halo = std::pow (1.0f - t, 2.6f) * (0.12f + 0.55f * light2D);
            }
            if (r >= 1.0f + 1.5f / discR)
                continue;

            const float z = std::sqrt (juce::jmax (0.0f, 1.0f - juce::jmin (1.0f, r * r)));
            const V3 n (x, y * ct - z * st, y * st + z * ct);   // Achse leicht geneigt
            const float lon = std::atan2 (n.x, n.z), lat = std::asin (juce::jlimit (-1.0f, 1.0f, n.y));
            const V3 eLon (std::cos (lon), 0.0f, -std::sin (lon));
            const V3 eLat (-std::sin (lon) * std::sin (lat), std::cos (lat), -std::cos (lon) * std::sin (lat));

            sp.u = (lon + juce::MathConstants<float>::pi) / juce::MathConstants<float>::twoPi * (float) PlutoTexture::W;
            sp.v = juce::jlimit (0.0f, (float) PlutoTexture::H - 1.001f,
                                 (juce::MathConstants<float>::halfPi - lat) / juce::MathConstants<float>::pi * (float) PlutoTexture::H - 0.5f);
            sp.dot = n * L1;
            sp.dotLon = eLon * L1;
            sp.dotLat = eLat * L1;
            sp.limb = 0.55f + 0.45f * std::sqrt (z);                                            // Randverdunkelung
            sp.haze = std::pow (1.0f - z, 3.0f) * juce::jlimit (0.0f, 1.0f, sp.dot + 0.3f);      // blauer Dunst am Rand
            sp.edge = juce::jlimit (0.0f, 1.0f, (1.0f - r) * discR + 0.5f);
        }
}

void PlanetButton::paint (juce::Graphics& g)
{
    const float glow = pressed ? 1.0f : (isMouseOver() ? 0.6f : 0.0f);
    const float R = radius() * (pressed ? 0.97f : 1.0f);

    // In 1,5-facher Auflösung rendern, damit es auch bei vergrößertem Fenster scharf bleibt
    const int size = (int) std::ceil (radius() * haloScale * 2.0f * 1.5f);
    if (size != sphereSize)
    {
        prepareSphere (size);
        frame = juce::Image (juce::Image::ARGB, size, size, true);
        frameShift = -1.0f;
    }

    const auto& tex = plutoTexture();
    const float shift = (float) (std::fmod (spin, juce::MathConstants<double>::twoPi) / juce::MathConstants<double>::twoPi) * (float) PlutoTexture::W;

    // Nur neu rechnen, wenn sich die Drehung um mindestens einen Texturpunkt geändert hat
    float moved = std::abs (shift - frameShift);
    moved = juce::jmin (moved, (float) PlutoTexture::W - moved);
    if (frameShift >= 0.0f && moved < 1.0f && std::abs (glow - frameGlow) < 0.001f)
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (frame, juce::Rectangle<float> (R * haloScale * 2.0f, R * haloScale * 2.0f).withCentre (getLocalBounds().toFloat().getCentre()));
        return;
    }
    frameShift = shift;
    frameGlow = glow;
    const Rgb haze = rgb (juce::Colour (0xff9fdcff));
    const float bump = 0.035f;

    {
        juce::Image::BitmapData bd (frame, juce::Image::BitmapData::writeOnly);
        for (int py = 0; py < size; ++py)
        {
            auto* line = bd.getLinePointer (py);
            for (int px = 0; px < size; ++px)
            {
                const auto& sp = sphere[(size_t) (py * size + px)];
                float cr = 0.0f, cg = 0.0f, cb = 0.0f, alpha = 0.0f;

                if (sp.edge > 0.0f)
                {
                    // Textur bilinear abtasten (Länge läuft rundherum)
                    float u = sp.u - shift;
                    u -= std::floor (u / (float) PlutoTexture::W) * (float) PlutoTexture::W;
                    const int i0 = (int) u % PlutoTexture::W, i1 = (i0 + 1) % PlutoTexture::W, j0 = (int) sp.v, j1 = j0 + 1;
                    const float fu = u - std::floor (u), fv = sp.v - (float) j0;
                    const auto idx = [] (int i, int j) { return (size_t) (j * PlutoTexture::W + i); };
                    const float w00 = (1 - fu) * (1 - fv), w10 = fu * (1 - fv), w01 = (1 - fu) * fv, w11 = fu * fv;
                    const auto& a00 = tex.albedo[idx (i0, j0)]; const auto& a10 = tex.albedo[idx (i1, j0)];
                    const auto& a01 = tex.albedo[idx (i0, j1)]; const auto& a11 = tex.albedo[idx (i1, j1)];
                    const float ar = a00.r * w00 + a10.r * w10 + a01.r * w01 + a11.r * w11;
                    const float ag = a00.g * w00 + a10.g * w10 + a01.g * w01 + a11.g * w11;
                    const float ab = a00.b * w00 + a10.b * w10 + a01.b * w01 + a11.b * w11;
                    const float gl = tex.gLon[idx (i0, j0)] * w00 + tex.gLon[idx (i1, j0)] * w10 + tex.gLon[idx (i0, j1)] * w01 + tex.gLon[idx (i1, j1)] * w11;
                    const float gt = tex.gLat[idx (i0, j0)] * w00 + tex.gLat[idx (i1, j0)] * w10 + tex.gLat[idx (i0, j1)] * w01 + tex.gLat[idx (i1, j1)] * w11;

                    // Licht: weiche Schattengrenze, Relief, Randverdunkelung
                    const float d = sp.dot - bump * (gl * sp.dotLon + gt * sp.dotLat);
                    const float lc = juce::jlimit (0.0f, 1.0f, (d + 0.06f) / 1.06f);
                    const float lit = lc * (0.85f + 0.15f * lc);
                    const float k = (0.03f + 1.08f * lit) * sp.limb * (1.0f + 0.12f * glow);
                    const float hz = sp.haze * (0.55f + 0.3f * glow);

                    cr = ar * k + haze.r * hz;
                    cg = ag * k + haze.g * hz;
                    cb = ab * k + haze.b * hz;
                    alpha = sp.edge;
                }

                if (sp.halo > 0.0f)
                {
                    const float a = sp.halo * (0.6f + 0.6f * glow);
                    const float add = a * (1.0f - alpha);
                    cr = cr * alpha + haze.r * add;
                    cg = cg * alpha + haze.g * add;
                    cb = cb * alpha + haze.b * add;
                    alpha += add;
                }
                else
                {
                    cr *= alpha; cg *= alpha; cb *= alpha;
                }

                auto* pix = (juce::PixelARGB*) (line + px * bd.pixelStride);
                const auto to8 = [] (float v) { return (juce::uint8) juce::jlimit (0, 255, (int) (v * 255.0f + 0.5f)); };
                pix->setARGB (to8 (alpha), to8 (juce::jmin (cr, alpha)), to8 (juce::jmin (cg, alpha)), to8 (juce::jmin (cb, alpha)));
            }
        }
    }

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    const auto c = getLocalBounds().toFloat().getCentre();
    g.drawImage (frame, juce::Rectangle<float> (R * haloScale * 2.0f, R * haloScale * 2.0f).withCentre (c));
}

//==============================================================================
IndikatPanel::IndikatPanel (PlutoProcessor& p)
    : proc (p)
{
    setOpaque (true);
    setLookAndFeel (&lnf);

    // Tonart und Tonleiter für alle tonalen Elemente
    auto addChoice = [this] (juce::ComboBox& box, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att,
                             const juce::String& id)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
            box.addItemList (choice->choices, 1);
        box.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (box);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, box);
    };
    addChoice (keyBox, keyAtt, "key");
    addChoice (scaleBox, scaleAtt, "scale");

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

IndikatPanel::~IndikatPanel()
{
    setLookAndFeel (nullptr);
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
    g.setFont (fonts->mono (10.0f).withExtraKerningFactor (0.08f));
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
                keyBox.getRight(), keyBox.getY(), scaleBox.getX() - keyBox.getRight(), keyBox.getHeight(),
                juce::Justification::centred);

    g.setFont (fonts->mono (10.0f).withExtraKerningFactor (0.3f));
    g.setColour (Palette::faint);
    g.drawText ("KEY", keyBox.getX(), keyBox.getY() - 14, 80, 12, juce::Justification::centredLeft);
    g.drawText ("SCALE", scaleBox.getX(), scaleBox.getY() - 14, 80, 12, juce::Justification::centredLeft);

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
    constexpr int cols = 5;
    const int rows = (PlutoEngine::numElements + cols - 1) / cols;
    const int rowH = grid.getHeight() / rows, colW = grid.getWidth() / cols;
    for (int e = 0; e < PlutoEngine::numElements; ++e)
        drawChip (g, { grid.getX() + (e % cols) * colW, grid.getY() + (e / cols) * rowH, colW, rowH },
                  e);
}

void IndikatPanel::resized()
{
    centre = { (float) getWidth() * 0.5f, 332.0f };
    planet.setBounds (juce::Rectangle<int> (284, 284).withCentre (centre.roundToInt()));
    layerBox = juce::Rectangle<int> (28, getHeight() - 28 - 150, getWidth() - 56, 150);
    keyBox.setBounds (34, layerBox.getY() - 40, 64, 24);
    scaleBox.setBounds (getWidth() - 34 - 120, layerBox.getY() - 40, 120, 24);
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
