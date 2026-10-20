#include "BlackHole.h"
#include "PluginEditor.h"

namespace
{
    const juce::Colour ice     { 0xff7fe6ff };
    const juce::Colour violet  { 0xffa292ff };
    const juce::Colour ember   { 0xffffb08a };
    const juce::Colour white   { 0xffeef4ff };
    const juce::Colour hair    { 0xff1a2333 };
    const juce::Colour dim     { 0xff66738a };
    const juce::Colour textCol { 0xffd2dbe8 };

    constexpr int texW = 512, texH = 64;

    inline float smoothstep (float a, float b, float x)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a));
        return t * t * (3.0f - 2.0f * t);
    }

    // Schnelles atan2 (Fehler < 0.01 rad), reicht für Texturkoordinaten
    inline float fastAtan2 (float y, float x)
    {
        const float ax = std::abs (x), ay = std::abs (y);
        const float a = juce::jmin (ax, ay) / (juce::jmax (ax, ay) + 1.0e-9f);
        const float s = a * a;
        float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
        if (ay > ax) r = 1.57079637f - r;
        if (x < 0.0f) r = 3.14159274f - r;
        return y < 0.0f ? -r : r;
    }
}

// Gruppenzuordnung bestimmt, welcher Effekt bei einer Änderung ausgelöst wird
static int groupIndexFor (const juce::String& id)
{
    if (id.startsWith ("drive")) return 2;
    if (id.startsWith ("delay") || id == "feedback" || id == "pingPong") return 3;
    if (id == "cutoff" || id == "reso" || id.startsWith ("env") || id == "filterType") return 1;
    if (id.startsWith ("pump")) return 4;
    if (id.startsWith ("osc") || id.startsWith ("amp") || id == "glide" || id == "legato"
        || id == "velocity" || id == "drift") return 5;
    return 0; // Low End, Pegel, Mix
}

//==============================================================================
BlackHole::BlackHole (DubBassProcessor& p) : proc (p)
{
    setOpaque (true);

    for (auto* param : proc.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            watched.push_back ({ ranged, ranged->getValue(), (Group) groupIndexFor (ranged->getParameterID()) });

    // Scheibentextur: periodisches Rauschen in Winkelrichtung, gestreckt (Schlieren)
    diskTex.resize ((size_t) (texW * texH));
    std::vector<float> grid (64 * 16);
    for (auto& v : grid) v = rng.nextFloat();
    auto value = [&] (float u, float v) // u periodisch über 64, v über 16
    {
        const int u0 = (int) std::floor (u), v0 = juce::jlimit (0, 15, (int) std::floor (v));
        const float fu = u - (float) u0, fv = v - std::floor (v);
        const int v1 = juce::jmin (15, v0 + 1);
        auto at = [&] (int uu, int vv) { return grid[(size_t) (((uu % 64 + 64) % 64) + vv * 64)]; };
        const float su = fu * fu * (3.0f - 2.0f * fu), sv = fv * fv * (3.0f - 2.0f * fv);
        const float a = at (u0, v0) + su * (at (u0 + 1, v0) - at (u0, v0));
        const float b = at (u0, v1) + su * (at (u0 + 1, v1) - at (u0, v1));
        return a + sv * (b - a);
    };
    for (int y = 0; y < texH; ++y)
        for (int x = 0; x < texW; ++x)
        {
            const float u = (float) x / texW, v = (float) y / texH;
            float n = 0.0f, amp = 0.5f, freq = 1.0f;
            for (int o = 0; o < 4; ++o)
            {
                n += amp * value (u * 8.0f * freq, v * 15.0f * freq * 0.5f + (float) o * 3.1f);
                amp *= 0.5f; freq *= 2.0f;
            }
            diskTex[(size_t) (x + y * texW)] = juce::jlimit (0.0f, 1.0f, (n - 0.2f) * 1.6f);
        }

    for (size_t i = 0; i < gaussTable.size(); ++i)
        gaussTable[i] = std::exp (-(float) i / 512.0f * 9.0f);

    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

BlackHole::~BlackHole() { stopTimer(); }

float BlackHole::norm (const char* id) const
{
    if (auto* p = proc.apvts.getParameter (id))
        return p->getValue();
    return 0.0f;
}

juce::Point<float> BlackHole::toPixels (float x, float y) const
{
    const auto b = getLocalBounds().toFloat();
    const float unit = b.getHeight() * 0.5f;
    return { b.getCentreX() + wobbleOffset.x + x * unit, b.getCentreY() + wobbleOffset.y + y * unit };
}

//==============================================================================
void BlackHole::detectParameterChanges()
{
    float maxDelta = 0.0f;
    int changedCount = 0;
    Watched* strongest = nullptr;
    float groupDelta[6] {};
    float groupSign[6] {};

    for (auto& w : watched)
    {
        const float v = w.param->getValue();
        const float d = v - w.last;
        if (std::abs (d) > 1.0e-4f)
        {
            const int g = (int) w.group;
            ++changedCount;
            groupDelta[g] += std::abs (d);
            groupSign[g] += d;
            if (std::abs (d) > maxDelta) { maxDelta = std::abs (d); strongest = &w; }
            w.last = v;
        }
    }

    if (strongest == nullptr)
        return;

    const bool programChange = changedCount >= 5; // Preset-Wechsel: viele Parameter auf einmal
    if (programChange)
    {
        hudName = "PROGRAM";
        hudValue = proc.getProgramName (proc.getCurrentProgram()).toUpperCase();
        rsVel -= 2.2f;          // Kollaps, danach federt die Masse zurück
        horizonFlash = 1.0f;
        hudAlpha = 1.6f;
    }
    else
    {
        hudName = strongest->param->getName (24).toUpperCase();
        hudValue = strongest->param->getCurrentValueAsText();
        hudAlpha = 1.0f;
    }

    float total = 0.0f;
    for (float d : groupDelta) total += d;
    total = juce::jmin (total, 3.0f);

    // Partikel: Farbe nach Gruppe, Anzahl nach Stärke der Änderung
    const juce::Colour groupColour[] = { white, ice, ember, violet, ice.interpolatedWith (white, 0.5f), white };
    for (int g = 0; g < 6; ++g)
    {
        if (groupDelta[g] <= 0.0f) continue;
        const int count = juce::jmin (40, 2 + (int) (groupDelta[g] * 140.0f));
        for (int i = 0; i < count && particles.size() < 420; ++i)
        {
            Particle pt;
            pt.r = rs * (2.2f + rng.nextFloat() * 1.8f);
            pt.a = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            pt.w = 0.8f + rng.nextFloat() * 0.6f;
            pt.life = 1.0f;
            pt.colour = (g == 1 && groupSign[g] < 0.0f) ? violet : groupColour[g];
            particles.push_back (pt);
        }
    }

    // Gruppenspezifische Effekte
    glitch = juce::jmin (1.5f, glitch + groupDelta[2] * 9.0f);
    flare = juce::jmin (2.0f, flare + groupDelta[1] * 6.0f);
    if (! programChange)
        rsVel += juce::jlimit (-1.2f, 1.2f, groupSign[0] * 5.0f) + groupDelta[5] * 0.8f;
    breathKick = juce::jmin (1.0f, breathKick + groupDelta[4] * 5.0f);
    spinBoost = juce::jmin (6.0f, spinBoost + total * 5.0f);
    burstFlash = juce::jmin (1.0f, burstFlash + total * 2.5f);

    if (waveCooldown <= 0.0f && total > 0.0015f)
    {
        waves.push_back ({ 0.0f, juce::jlimit (0.25f, 1.0f, total * 12.0f), groupColour[(int) strongest->group] });
        if (groupDelta[3] > 0.0f) // Delay: gestaffelte Echo-Wellen
            for (int k = 1; k <= 3; ++k)
                waves.push_back ({ -0.16f * (float) k, 0.7f / (float) k, violet });
        waveCooldown = 0.09f;
    }
}

void BlackHole::updateVisualState (float dt)
{
    auto approach = [dt] (float& v, float target, float speed) { v += (target - v) * juce::jmin (1.0f, dt * speed); };

    const float peak = proc.outLevel.load();
    const float env = juce::jlimit (0.0f, 1.0f, proc.envDisplay.load() * 2.0f);
    const float pumpGain = proc.pumpDisplay.load();

    approach (level, juce::jlimit (0.0f, 1.0f, juce::jmap (juce::Decibels::gainToDecibels (peak, -60.0f), -48.0f, 0.0f, 0.0f, 1.0f)), 12.0f);
    approach (envFlare, env, 20.0f);
    approach (duck, 1.0f - pumpGain, 25.0f);

    // Parameter -> Aussehen
    approach (heat, norm ("cutoff"), 8.0f);
    approach (crunch, norm ("drive") * (0.3f + 0.7f * norm ("driveMix")), 8.0f);
    approach (ringAmt, 0.35f + 1.2f * norm ("reso"), 8.0f);
    approach (ringWidth, 0.10f - 0.075f * norm ("reso"), 8.0f);
    approach (tilt, 0.15f + 0.20f * (1.0f - norm ("monoFreq")), 6.0f);
    approach (echoCount, std::round (norm ("feedback") * 5.0f), 6.0f);
    approach (echoAmt, norm ("delayMix"), 8.0f);
    approach (echoDecay, 0.35f + 0.6f * norm ("feedback"), 8.0f);
    approach (diskAmt, 0.3f + 0.7f * norm ("mix"), 8.0f);
    approach (rout, 3.0f + 1.4f * norm ("feedback") * norm ("delayMix") + 0.6f * norm ("driveMix") * norm ("drive"), 6.0f);

    if (DubBassProcessor::isSynth)
    {
        texStyle = (int) std::round (norm ("osc1Wave") * 4.0f);
        approach (wobble, norm ("drift"), 6.0f);
        approach (binary, norm ("osc2Level") * juce::jmin (1.0f, std::abs (norm ("osc2Fine") - 0.5f) * 4.0f
                                                                 + std::abs (norm ("osc2Semi") - 0.5f) * 2.0f), 6.0f);
    }

    // Masse als Feder: Zielgröße aus Sub, Pegel, Low Cut; reagiert auf Audio und Pump
    const float rsTarget = 0.30f + 0.16f * norm ("subGain") + 0.07f * norm ("inGain") + 0.05f * (1.0f - norm ("lowCut"))
                         + 0.06f * level - 0.06f * duck * (0.5f + 0.5f * norm ("pumpDepth")) - 0.05f * breathKick;
    const float acc = 70.0f * (rsTarget - rs) - 7.5f * rsVel;
    rsVel += acc * dt;
    rs = juce::jlimit (0.16f, 0.68f, rs + rsVel * dt);

    spinPhase += dt * (0.35f + 0.9f * std::abs (norm ("envAmt") - 0.5f) * 2.0f + spinBoost + envFlare * 0.8f);
    if (spinPhase > 1000.0f) spinPhase -= 1000.0f;

    // Abklingen der Kicks
    const float decay = std::exp (-dt * 3.5f);
    flare *= decay; glitch *= std::exp (-dt * 5.0f); breathKick *= decay; burstFlash *= std::exp (-dt * 6.0f);
    spinBoost *= std::exp (-dt * 2.0f); horizonFlash *= std::exp (-dt * 4.0f);
    hudAlpha = juce::jmax (0.0f, hudAlpha - dt * 0.6f);
    waveCooldown -= dt;

    // Wackeln (Drift)
    wobbleOffset = { std::sin (time * 1.7f) * wobble * 1.6f + std::sin (time * 4.3f) * wobble * 0.6f,
                     std::cos (time * 1.3f) * wobble * 1.1f };

    // Partikel: Keplerbahn mit Einfall
    for (auto& pt : particles)
    {
        pt.a += dt * pt.w * 0.9f / std::pow (juce::jmax (0.05f, pt.r), 1.5f) * (1.0f + spinBoost * 0.15f);
        pt.r -= dt * (0.05f + 0.035f / std::sqrt (juce::jmax (0.05f, pt.r)));
        if (pt.r < rs * 1.02f)
        {
            pt.life = 0.0f;
            horizonFlash = juce::jmin (1.0f, horizonFlash + 0.03f);
        }
    }
    particles.erase (std::remove_if (particles.begin(), particles.end(), [] (const Particle& pt) { return pt.life <= 0.0f; }),
                     particles.end());

    for (auto& w : waves) w.age += dt;
    waves.erase (std::remove_if (waves.begin(), waves.end(), [] (const Wave& w) { return w.age > 1.1f; }), waves.end());
}

//==============================================================================
void BlackHole::ensureRaster()
{
    const float scale = juce::jlimit (1.0f, 2.0f, juce::Component::getApproximateScaleFactorForComponent (this));
    int w = juce::roundToInt ((float) getWidth() * scale), h = juce::roundToInt ((float) getHeight() * scale);
    const float maxPixels = 110000.0f;
    if ((float) (w * h) > maxPixels)
    {
        const float f = std::sqrt (maxPixels / (float) (w * h));
        w = (int) ((float) w * f); h = (int) ((float) h * f);
    }
    if (w == rw && h == rh && raster.isValid())
        return;

    rw = juce::jmax (8, w); rh = juce::jmax (8, h);
    rasterScale = (float) rw / (float) juce::jmax (1, getWidth());
    raster = juce::Image (juce::Image::ARGB, rw, rh, true);
    planeR.assign ((size_t) (rw * rh), 0.0f);
    planeG.assign ((size_t) (rw * rh), 0.0f);
    planeB.assign ((size_t) (rw * rh), 0.0f);

    // Sternenfeld mit schwachem Nebel
    stars.assign ((size_t) (rw * rh * 3), 0.0f);
    juce::Random srng (0xb1ac);
    const int count = rw * rh / 190;
    for (int i = 0; i < count; ++i)
    {
        const int x = srng.nextInt (rw), y = srng.nextInt (rh);
        const float b = std::pow (srng.nextFloat(), 5.0f) * 0.75f + 0.03f;
        const bool tint = srng.nextFloat() < 0.2f;
        auto* s = &stars[(size_t) ((x + y * rw) * 3)];
        s[0] = b * (tint ? 0.6f : 0.95f); s[1] = b * (tint ? 0.9f : 0.97f); s[2] = b;
    }
    for (int y = 0; y < rh; ++y)
        for (int x = 0; x < rw; ++x)
        {
            const float u = (float) x / (float) rw, v = (float) y / (float) rh;
            const float neb = 0.018f * (0.5f + 0.5f * std::sin (u * 7.0f + std::sin (v * 5.0f) * 1.5f))
                                     * (0.5f + 0.5f * std::cos (v * 6.0f - u * 3.0f));
            auto* s = &stars[(size_t) ((x + y * rw) * 3)];
            s[0] += neb * 0.5f; s[1] += neb * 0.8f; s[2] += neb * 1.4f;
        }
}

void BlackHole::buildColourMap()
{
    // Emission -> Farbe: tiefes Blau -> Eis-Cyan -> Weiß; Cutoff = Temperatur, Crunch färbt die Spitzen
    struct Stop { float pos, r, g, b; };
    const Stop cool[] = { { 0.0f, 0, 0, 0 }, { 0.18f, 0.03f, 0.07f, 0.24f }, { 0.45f, 0.22f, 0.55f, 0.95f },
                          { 0.72f, 0.50f, 0.90f, 1.0f }, { 1.0f, 0.95f, 0.99f, 1.0f } };
    const float temp = 0.45f + 0.75f * heat;

    for (int i = 0; i < 256; ++i)
    {
        const float x = juce::jlimit (0.0f, 1.0f, (float) i / 255.0f * temp);
        int s = 0;
        while (s < 3 && x > cool[s + 1].pos) ++s;
        const float t = (x - cool[s].pos) / (cool[s + 1].pos - cool[s].pos);
        float r = cool[s].r + t * (cool[s + 1].r - cool[s].r);
        float g = cool[s].g + t * (cool[s + 1].g - cool[s].g);
        float b = cool[s].b + t * (cool[s + 1].b - cool[s].b);

        // Crunch: Spitzen werden heißer (Weiß mit Violett/Ember-Stich), Mitten satter
        const float hot = crunch * smoothstep (0.3f, 1.0f, x);
        r += hot * (0.95f - r) * 0.8f; g += hot * (0.80f - g) * 0.5f; b += hot * (1.0f - b) * 0.6f;

        colourMap[(size_t) (i * 3)] = r; colourMap[(size_t) (i * 3 + 1)] = g; colourMap[(size_t) (i * 3 + 2)] = b;
    }
}

void BlackHole::renderRaster()
{
    buildColourMap();

    const float unit = (float) rh * 0.5f;
    const float cx = (float) rw * 0.5f + wobbleOffset.x * rasterScale;
    const float cy = (float) rh * 0.5f + wobbleOffset.y * rasterScale;
    const float R = rs, R2 = R * R;
    const float rin = R * 1.55f, outer = R * rout;
    const float invTilt = 1.0f / tilt;
    const float lens = 1.5f;
    const float brightness = (0.75f + 0.5f * level + 0.6f * envFlare + 0.6f * flare + 0.4f * burstFlash) * (1.0f - 0.35f * duck);
    const float ringA = ringAmt * (1.0f + flare * 0.6f + horizonFlash * 1.5f);
    const float ringInvW = 1.0f / (R * ringWidth);
    const int echoes = juce::roundToInt (echoCount);
    const float binOffset = binary * R * 0.12f;
    const float twoPiInv = 1.0f / juce::MathConstants<float>::twoPi;

    auto gauss = [this] (float d) // exp(-d^2) über Tabelle
    {
        const float x = d * d;
        return x >= 9.0f ? 0.0f : gaussTable[(size_t) (x * (512.0f / 9.0f))];
    };
    auto tex = [this] (float angle, float radial)
    {
        int u = (int) (angle * texW) % texW; if (u < 0) u += texW;
        const int v = juce::jlimit (0, texH - 1, (int) (radial * texH));
        return diskTex[(size_t) (u + v * texW)];
    };
    auto shape = [this] (float t)
    {
        switch (texStyle)
        {
            case 0:  return 0.35f + 0.4f * t;                          // Sine: weich
            case 1:  return 0.2f + 0.8f * t;                           // Triangle
            case 3:  return std::floor (t * 4.0f) * 0.3f + 0.1f;       // Square: Bänder
            case 4:  return t > 0.62f ? 1.2f : 0.15f;                  // Pulse: Punkte
            default: return 0.1f + 1.1f * t * t;                       // Saw: Schlieren
        }
    };

    for (int y = 0; y < rh; ++y)
    {
        const float dy = ((float) y - cy) / unit;
        for (int x = 0; x < rw; ++x)
        {
            const float dx = ((float) x - cx) / unit;
            const float r2 = dx * dx + dy * dy;
            const float r = std::sqrt (r2);
            const size_t idx = (size_t) (x + y * rw);

            float br = 0.0f, bgc = 0.0f, bb = 0.0f;
            float e = 0.0f;

            if (r > R)
            {
                // Gravitationslinse für den Hintergrund
                const float f = 1.0f - lens * R2 / r2;
                int sx = (int) (cx + dx * f * unit), sy = (int) (cy + dy * f * unit);
                sx = ((sx % rw) + rw) % rw; sy = ((sy % rh) + rh) % rh;
                const float* s = &stars[(size_t) ((sx + sy * rw) * 3)];
                const float fade = smoothstep (R, R * 1.5f, r);
                br = s[0] * fade; bgc = s[1] * fade; bb = s[2] * fade;

                // Gelinstes Bild der Scheibenrückseite (Halo um den Schatten)
                const float theta = fastAtan2 (dy, dx);
                const float halo = gauss ((r - R * 1.30f) / (R * 0.20f));
                if (halo > 0.001f)
                    e += halo * (0.25f + 0.75f * std::abs (dy) / r) * shape (tex (theta * twoPiInv - spinPhase * 0.11f, 0.35f)) * 0.9f;

                // Photonenring (+ Doppelring bei verstimmtem OSC 2)
                e += ringA * gauss ((r - R * 1.03f) * ringInvW);
                if (binOffset > 0.0005f)
                {
                    const float dxb = dx - binOffset;
                    const float rb = std::sqrt (dxb * dxb + dy * dy);
                    e += ringA * 0.45f * binary * gauss ((rb - R * 1.06f) * ringInvW);
                }

                // Echo-Ringe (Delay)
                float amp = echoAmt;
                for (int k = 1; k <= echoes; ++k)
                {
                    amp *= echoDecay;
                    e += amp * 0.7f * gauss ((r - R * (1.03f + 0.42f * (float) k)) / (R * 0.05f));
                }
            }

            // Akkretionsscheibe (Vorderseite verdeckt den Schatten, Rückseite liegt dahinter)
            const float v = dy * invTilt;
            const float rd = std::sqrt (dx * dx + v * v);
            if (rd > rin * 0.85f && rd < outer && ! (dy < 0.0f && r < R * 1.02f))
            {
                const float phi = fastAtan2 (v, dx);
                const float prof = smoothstep (rin * 0.85f, rin * 1.2f, rd) * (1.0f - smoothstep (outer * 0.5f, outer, rd))
                                 * (rin / rd);
                const float swirl = phi * twoPiInv - spinPhase * 0.25f / (rd * std::sqrt (rd) + 0.05f) * R * 2.0f;
                const float t = shape (tex (swirl, (rd - rin) / (outer - rin)));
                const float doppler = 1.0f - 0.32f * (dx / rd);
                e += prof * (0.3f + t) * doppler * doppler * diskAmt;
            }

            e *= brightness;
            const int ci = juce::jlimit (0, 255, (int) (e * 255.0f));
            planeR[idx] = br + colourMap[(size_t) (ci * 3)];
            planeG[idx] = bgc + colourMap[(size_t) (ci * 3 + 1)];
            planeB[idx] = bb + colourMap[(size_t) (ci * 3 + 2)];
        }
    }

    // Crunch: Farbversatz, Glitch-Streifen, Körnung, Bitcrush-Stufen
    const float chroma = (crunch * 1.4f + glitch * 3.0f) * rasterScale;
    const int shift = (int) chroma;
    const float grain = crunch * 0.07f + glitch * 0.06f;
    const float levels = 255.0f - (255.0f - 14.0f) * juce::jlimit (0.0f, 1.0f, crunch * crunch * 0.85f + glitch * 0.25f);
    const bool posterize = levels < 250.0f;

    juce::Image::BitmapData bd (raster, juce::Image::BitmapData::writeOnly);
    int sliceOffset = 0, sliceRowsLeft = 0;
    for (int y = 0; y < rh; ++y)
    {
        if (sliceRowsLeft-- <= 0)
        {
            sliceOffset = 0;
            if (rng.nextFloat() < glitch * 0.08f + crunch * 0.01f)
            {
                sliceOffset = (int) ((rng.nextFloat() * 2.0f - 1.0f) * (8.0f + 30.0f * glitch) * rasterScale);
                sliceRowsLeft = 1 + rng.nextInt (juce::jmax (2, (int) (6.0f * rasterScale)));
            }
        }

        for (int x = 0; x < rw; ++x)
        {
            const int xs = juce::jlimit (0, rw - 1, x + sliceOffset);
            const int row = y * rw;
            float r = planeR[(size_t) (row + juce::jlimit (0, rw - 1, xs - shift))];
            float g = planeG[(size_t) (row + xs)];
            float b = planeB[(size_t) (row + juce::jlimit (0, rw - 1, xs + shift))];

            if (grain > 0.002f)
            {
                const float n = (rng.nextFloat() - 0.5f) * grain;
                r += n; g += n; b += n;
            }
            auto toByte = [&] (float c)
            {
                c = juce::jlimit (0.0f, 1.0f, c);
                if (posterize) c = std::round (c * levels) / levels;
                return (juce::uint8) (c * 255.0f);
            };
            auto* px = reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (x, y));
            px->setARGB (255, toByte (r), toByte (g), toByte (b));
        }
    }
}

//==============================================================================
void BlackHole::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.001, 0.05, (now - lastTime) * 0.001);
    lastTime = now;
    time += dt;

    detectParameterChanges();
    updateVisualState (dt);
    repaint();
}

void BlackHole::paint (juce::Graphics& g)
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    ensureRaster();
    renderRaster();

    const auto bounds = getLocalBounds().toFloat();
    g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    g.drawImage (raster, bounds, juce::RectanglePlacement::stretchToFit);

    const float unit = bounds.getHeight() * 0.5f;
    const auto centre = toPixels (0.0f, 0.0f);

    // Schockwellen
    for (auto& w : waves)
    {
        if (w.age < 0.0f) continue;
        const float radius = (rs + w.age * 0.9f) * unit;
        const float alpha = w.strength * (1.0f - w.age / 1.1f);
        const auto ring = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        g.setColour (w.colour.withAlpha (alpha * 0.12f));
        g.drawEllipse (ring, 6.0f);
        g.setColour (w.colour.withAlpha (alpha * 0.7f));
        g.drawEllipse (ring, 1.0f);
    }

    // Partikel als Leuchtspuren
    for (auto& pt : particles)
    {
        const auto pos = toPixels (pt.r * std::cos (pt.a), pt.r * std::sin (pt.a) * tilt);
        const bool hidden = std::sin (pt.a) < 0.0f && pos.getDistanceFrom (centre) < rs * unit * 1.05f;
        if (! hidden && pt.hasPrev)
        {
            const float a = juce::jlimit (0.0f, 1.0f, (pt.r / rs - 1.0f) * 0.8f);
            g.setColour (pt.colour.withAlpha (0.25f * a));
            g.drawLine (juce::Line<float> (pt.prev, pos), 3.0f);
            g.setColour (pt.colour.withAlpha (0.9f * a));
            g.drawLine (juce::Line<float> (pt.prev, pos), 1.0f);
        }
        pt.prev = pos;
        pt.hasPrev = ! hidden;
    }

    // Rahmen mit Eckmarken
    g.setColour (hair);
    g.drawRect (bounds, 1.0f);
    g.setColour (dim);
    const float m = 8.0f, l = bounds.getX(), t = bounds.getY(), r = bounds.getRight() - 1.0f, b = bounds.getBottom() - 1.0f;
    g.fillRect (l, t, m, 1.0f);     g.fillRect (l, t, 1.0f, m);
    g.fillRect (r - m + 1.0f, t, m, 1.0f); g.fillRect (r, t, 1.0f, m);
    g.fillRect (l, b, m, 1.0f);     g.fillRect (l, b - m + 1.0f, 1.0f, m);
    g.fillRect (r - m + 1.0f, b, m, 1.0f); g.fillRect (r, b - m + 1.0f, 1.0f, m);

    // HUD
    static juce::SharedResourcePointer<IndikatFonts> fonts;
    g.setFont (fonts->mono (9.5f).withExtraKerningFactor (0.15f));
    g.setColour (dim);
    auto inner = bounds.reduced (8.0f, 5.0f);
    g.drawText ("SINGULARITY", inner, juce::Justification::topLeft);
    g.drawText ("M " + juce::String (1.0f + rs * 12.0f, 2), inner, juce::Justification::topRight);
    g.drawText ("SPIN " + juce::String (juce::jmin (0.99f, 0.3f + spinBoost * 0.1f + envFlare * 0.3f), 2), inner,
                juce::Justification::bottomLeft);
    g.drawText ("T " + juce::String (juce::roundToInt (2000.0f + heat * 9800.0f + crunch * 1500.0f)) + " K", inner,
                juce::Justification::bottomRight);

    if (hudAlpha > 0.0f)
    {
        const float a = juce::jmin (1.0f, hudAlpha * 1.6f);
        auto label = inner.withTrimmedTop (inner.getHeight() - 12.0f).reduced (70.0f, 0.0f);
        g.setColour (textCol.withAlpha (a));
        g.drawText (hudName + "  " + hudValue, label, juce::Justification::centred);
    }
}
