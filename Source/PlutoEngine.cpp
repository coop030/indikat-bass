#include "PlutoEngine.h"

namespace
{
    float decayCoeff (float seconds, double sr) { return std::exp (-1.0f / (seconds * (float) sr)); }

    struct Scale { const char* name; int size; int steps[7]; };
    constexpr Scale scales[PlutoEngine::numScales] = {
        { "MINOR",      7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "MAJOR",      7, { 0, 2, 4, 5, 7, 9, 11 } },
        { "DORIAN",     7, { 0, 2, 3, 5, 7, 9, 10 } },
        { "PHRYGIAN",   7, { 0, 1, 3, 5, 7, 8, 10 } },
        { "MIN PENTA",  5, { 0, 3, 5, 7, 10 } },
        { "MAJ PENTA",  5, { 0, 2, 4, 7, 9 } },
        { "HARM MINOR", 7, { 0, 2, 3, 5, 7, 8, 11 } },
        { "BLUES",      6, { 0, 3, 5, 6, 7, 10 } }
    };

    // Wählt count verschiedene Indizes, gewichtet
    template <size_t N>
    std::array<bool, N> pickDistinct (juce::Random& r, std::array<float, N> weights, int count)
    {
        std::array<bool, N> chosen {};
        for (int c = 0; c < count; ++c)
        {
            float total = 0.0f;
            for (size_t i = 0; i < N; ++i) if (! chosen[i]) total += weights[i];
            float pick = r.nextFloat() * total;
            for (size_t i = 0; i < N; ++i)
            {
                if (chosen[i]) continue;
                if (pick < weights[i] || i == N - 1) { chosen[i] = true; break; }
                pick -= weights[i];
            }
        }
        return chosen;
    }
}

const char* PlutoEngine::elementName (int e)
{
    static const char* names[] = { "VINYL", "HISS", "SOLAR WIND", "RAIN", "RUMBLE", "SHORTWAVE", "GEIGER", "CRICKETS", "BUBBLES",
                                   "BLIPS", "DRIPS", "PINGS", "STATIC", "GLITCH", "SWEEPS", "MORSE", "DATA", "THUNDER", "SWOOSH", "CREAK" };
    return juce::isPositiveAndBelow (e, (int) numElements) ? names[e] : "";
}

const char* PlutoEngine::scaleName (int i) { return scales[juce::jlimit (0, numScales - 1, i)].name; }

const char* PlutoEngine::noteName (int i)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((i % 12) + 12) % 12];
}

float PlutoEngine::scaleFreq (int octaveLow, int octaveHigh)
{
    const auto& sc = scales[juce::jlimit (0, numScales - 1, scaleNow)];
    const int octave = octaveLow + rng.nextInt (octaveHigh - octaveLow + 1);
    const int midi = 12 * (octave + 1) + rootNow + sc.steps[rng.nextInt (sc.size)];
    return 440.0f * std::pow (2.0f, (float) (midi - 69) / 12.0f);
}

PlutoEngine::Scene PlutoEngine::makeScene (juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed);
    Scene s;

    // 1–3 Flächen, Vinyl am wahrscheinlichsten
    const int numBedsChosen = 1 + (r.nextFloat() < 0.75f ? 1 : 0) + (r.nextFloat() < 0.35f ? 1 : 0);
    const auto beds = pickDistinct<numBeds> (r, { 3.0f, 1.4f, 2.0f, 1.5f, 1.2f, 0.8f, 1.6f, 1.2f, 1.0f }, numBedsChosen);
    for (int i = 0; i < numBeds; ++i)
        s.bedGain[(size_t) i] = beds[(size_t) i] ? 0.55f + 0.45f * r.nextFloat() : 0.0f;

    // 1–3 Ereignis-Typen
    const int numEventsChosen = 1 + r.nextInt (3);
    // tonale Elemente (Blips, Pings, Morse) etwas seltener als Geräusche
    const auto events = pickDistinct<numEventTypes> (r, { 1.2f, 1.6f, 1.0f, 1.3f, 1.5f, 0.8f, 1.0f, 1.4f, 1.0f, 1.2f, 1.3f },
                                                     numEventsChosen);
    for (int i = 0; i < numEventTypes; ++i)
        s.eventWeight[(size_t) i] = events[(size_t) i] ? 0.4f + 0.6f * r.nextFloat() : 0.0f;
    s.eventDensity = 0.02f + 0.07f * std::pow (r.nextFloat(), 1.4f);

    s.crackleRate = 8.0f + 32.0f * r.nextFloat();
    s.rainRate = 18.0f + 70.0f * r.nextFloat();
    s.windCentre = 0.6f + r.nextFloat();
    s.rumbleFreq = 70.0f + 90.0f * r.nextFloat();
    s.geigerRate = 3.0f + 22.0f * r.nextFloat();
    s.cricketFreq = 3800.0f + 1800.0f * r.nextFloat();
    s.bubbleRate = 1.5f + 6.0f * r.nextFloat();
    s.blipOctave = r.nextFloat() < 0.3f ? 5 : 6;

    constexpr int delayPairs[][2] = { { 3, 4 }, { 3, 6 }, { 4, 6 }, { 2, 3 }, { 6, 8 }, { 3, 3 } };
    const auto& d = delayPairs[r.nextInt ((int) std::size (delayPairs))];
    s.delayStepsL = d[0];
    s.delayStepsR = d[1];
    s.feedback = 0.35f + 0.33f * r.nextFloat();
    s.verbSend = 0.3f + 0.7f * r.nextFloat();
    s.roomSize = 0.8f + 0.15f * r.nextFloat();
    s.tone = 3500.0f * std::pow (4.0f, r.nextFloat());
    s.width = 0.6f + 0.4f * r.nextFloat();
    return s;
}

void PlutoEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    const juce::dsp::ProcessSpec mono { sr, (juce::uint32) juce::jmax (1, maxBlockSize), 1 };

    for (int ch = 0; ch < 2; ++ch)
    {
        hissHP[ch].setLowpass (1500.0f, sr);
        hissLP[ch].setLowpass (7000.0f, sr);
        crackleHP[ch].setLowpass (900.0f, sr);
        popLP[ch].setLowpass (1800.0f, sr);
        rainLP[ch].setLowpass (4500.0f, sr);
        rumbleHP[ch].setLowpass (45.0f, sr);
        delayHP[ch].setLowpass (380.0f, sr);
        delayLP[ch].setLowpass (2600.0f, sr);
        outHP[ch].setLowpass (40.0f, sr);

        for (auto& f : windBand[ch])
        {
            f.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            f.prepare (mono);
            f.setResonance (3.2f);
        }
    }
    swNoise.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    swNoise.prepare (mono);
    swNoise.setResonance (4.0f);

    crackleDecay = decayCoeff (0.00018f, sr);
    popDecay = decayCoeff (0.0016f, sr);
    geigerHP.setLowpass (1800.0f, sr);
    geigerDecay = decayCoeff (0.00008f, sr);
    geigerRingDecay = decayCoeff (0.0009f, sr);
    bedSmoothing = 1.0f - decayCoeff (0.8f, sr);

    for (auto& v : voices)
    {
        v.bp.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        v.bp.prepare (mono);
    }

    delayBuf.setSize (2, (int) (sr * 4.0) + 4);
    verbBuf.setSize (2, juce::jmax (1, maxBlockSize));
    reverb.prepare ({ sr, (juce::uint32) juce::jmax (1, maxBlockSize), 2 });

    reset();
}

void PlutoEngine::reset()
{
    rng.setSeedRandomly();
    for (int ch = 0; ch < 2; ++ch)
    {
        hissHP[ch].z = hissLP[ch].z = crackleHP[ch].z = popLP[ch].z = rainLP[ch].z = rumbleHP[ch].z = 0.0f;
        rumbleLP[ch][0].z = rumbleLP[ch][1].z = 0.0f;
        delayHP[ch].z = delayLP[ch].z = toneLP[ch].z = outHP[ch].z = 0.0f;
        crackleEnv[ch] = popEnv[ch] = 0.0f;
        for (auto& s : pinkState[ch]) s = 0.0f;
        for (int b = 0; b < 2; ++b)
        {
            windBand[ch][b].reset();
            windFreq[ch][b] = windTarget[ch][b] = 500.0f + 900.0f * (float) b + 300.0f * (float) ch;
        }
    }
    swNoise.reset();
    for (auto& dr : drops) dr.env = 0.0f;
    for (auto& b : bubbles) b.env = 0.0f;
    geigerEnv = geigerRingEnv = geigerHP.z = 0.0f;
    for (size_t i = 0; i < crickets.size(); ++i)
    {
        crickets[i].pos = 0;
        crickets[i].pan = 0.2f + 0.3f * (float) i;
    }
    for (auto& v : voices) { v.active = false; v.bp.reset(); }
    delayBuf.clear();
    verbBuf.clear();
    reverb.reset();
    delayWrite = 0;
    delayTimeL = delayTimeR = 0.0f;
    stepPhase = 0.0;
    lastPpqStep = -1;
    bedLevel = {};
    shown = {};

    // Aktuelle Szene beibehalten bzw. die angeforderte übernehmen
    if (pendingSeed.load() < 0)
        pendingSeed.store ((juce::int64) display.seed.load());
}

float PlutoEngine::pink (int ch)
{
    // Paul Kellets Näherung
    auto& b = pinkState[ch];
    const float w = noise();
    b[0] = 0.99765f * b[0] + w * 0.0990460f;
    b[1] = 0.96300f * b[1] + w * 0.2965164f;
    b[2] = 0.57000f * b[2] + w * 1.0526913f;
    return (b[0] + b[1] + b[2] + w * 0.1848f) * 0.22f;
}

void PlutoEngine::publishEvent (int type, float pan)
{
    const int n = display.eventCount.load (std::memory_order_relaxed);
    const auto idx = (size_t) (n % Display::ringSize);
    display.eventType[idx].store (type, std::memory_order_relaxed);
    display.eventPan[idx].store (pan, std::memory_order_relaxed);
    display.eventCount.store (n + 1, std::memory_order_release);
}

void PlutoEngine::triggerEvent (double samplesPerStep)
{
    Voice* v = nullptr;
    for (auto& candidate : voices)
        if (! candidate.active) { v = &candidate; break; }
    if (v == nullptr)
        return;

    float total = 0.0f;
    for (auto w : scene.eventWeight) total += w;
    if (total <= 0.0f)
        return;
    float pick = rng.nextFloat() * total;
    int type = 0;
    while (type < numEventTypes - 1 && pick >= scene.eventWeight[(size_t) type]) { pick -= scene.eventWeight[(size_t) type]; ++type; }

    auto& voice = *v;
    voice.active = true;
    voice.type = numBeds + type;
    voice.age = 0;
    voice.phase = 0.0;
    voice.pan = 0.15f + 0.7f * rng.nextFloat();

    const auto step = (int) samplesPerStep;
    switch (voice.type)
    {
        case Blips:
            voice.freq = scaleFreq (scene.blipOctave, scene.blipOctave + 1);
            voice.repeatLen = juce::jmax (1, step / 2);
            voice.length = (1 + rng.nextInt (3)) * voice.repeatLen;
            voice.gain = 0.045f;
            voice.sendDelay = 0.75f;
            voice.sendVerb = 0.3f;
            break;

        case Drips:
            voice.freq = scaleFreq (5, 5);
            voice.freqEnd = voice.freq * 2.0f;
            voice.length = (int) (sr * 0.25);
            voice.gain = 0.09f;
            voice.sendDelay = 0.85f;
            voice.sendVerb = 0.35f;
            break;

        case Pings:
            voice.freq = scaleFreq (3, 4);
            voice.length = (int) (sr * 1.6);
            voice.gain = 0.035f;
            voice.sendDelay = 0.4f;
            voice.sendVerb = 0.9f;
            break;

        case Static:
            voice.freq = 900.0f + 2600.0f * rng.nextFloat();
            voice.chop = 14.0f + 46.0f * rng.nextFloat();
            voice.length = (int) (sr * (0.3 + 0.9 * rng.nextDouble()));
            voice.gain = 0.11f;
            voice.sendDelay = 0.2f;
            voice.sendVerb = 0.6f;
            voice.bp.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            voice.bp.reset();
            voice.bp.setCutoffFrequency (voice.freq);
            voice.bp.setResonance (2.0f);
            break;

        case Glitch:
            voice.repeatLen = juce::jmax (1, rng.nextBool() ? step / 2 : step / 4);
            voice.length = (2 + rng.nextInt (5)) * voice.repeatLen;
            voice.holdPeriod = juce::jmax (1, (int) (sr / (1500.0 + 7000.0 * rng.nextDouble())));
            voice.holdCounter = 0;
            voice.bitSteps = (float) (3 + rng.nextInt (6));
            voice.gain = 0.05f;
            voice.sendDelay = 0.5f;
            voice.sendVerb = 0.12f;
            break;

        case Morse:
        {
            // Zufällige Zeichen aus Punkten (1 Einheit) und Strichen (3 Einheiten), Einheit = 1/32
            voice.freq = scaleFreq (5, 6);
            voice.repeatLen = juce::jmax (1, step / 2);
            voice.pattern = 0;
            int unit = 0;
            const int symbols = 5 + rng.nextInt (8);
            for (int k = 0; k < symbols && unit < 60; ++k)
            {
                const int on = rng.nextFloat() < 0.6f ? 1 : 3;
                for (int u = 0; u < on && unit < 64; ++u) voice.pattern |= (juce::uint64) 1 << unit++;
                unit += rng.nextFloat() < 0.25f ? 3 : 1;
            }
            voice.length = unit * voice.repeatLen;
            voice.envS = 0.0f;
            voice.gain = 0.02f;
            voice.sendDelay = 0.6f;
            voice.sendVerb = 0.25f;
            break;
        }

        case Data:
            voice.holdPeriod = juce::jmax (1, (int) (sr * (0.006 + 0.01 * rng.nextDouble())));
            voice.holdCounter = 0;
            voice.length = (int) (sr * (0.15 + 0.45 * rng.nextDouble()));
            voice.gain = 0.018f;
            voice.sendDelay = 0.45f;
            voice.sendVerb = 0.15f;
            break;

        case Thunder:
            voice.length = (int) (sr * (3.0 + 3.0 * rng.nextDouble()));
            voice.freq = 0.3f + 0.6f * rng.nextFloat();    // Anstiegszeit in s
            voice.gain = 0.5f;
            voice.sendDelay = 0.0f;
            voice.sendVerb = 0.6f;
            voice.envS = 0.0f;
            voice.pan = 0.3f + 0.4f * rng.nextFloat();
            voice.bp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
            voice.bp.reset();
            voice.bp.setCutoffFrequency (140.0f + 260.0f * rng.nextFloat());
            voice.bp.setResonance (0.8f);
            break;

        case Swoosh:
            // Rückwärts-Hallfahne, endet auf einem Schlag
            voice.length = juce::jmax (1, step * (rng.nextBool() ? 4 : 8));
            voice.freq = 250.0f + 200.0f * rng.nextFloat();
            voice.freqEnd = 3000.0f + 3000.0f * rng.nextFloat();
            voice.gain = 0.09f;
            voice.sendDelay = 0.1f;
            voice.sendVerb = 0.8f;
            voice.bp.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            voice.bp.reset();
            voice.bp.setResonance (1.5f);
            break;

        case Creak:
            // Rumpf-Knarzen: ruckelnde Impulse durch einen engen Resonanzfilter
            voice.length = (int) (sr * (0.4 + 0.9 * rng.nextDouble()));
            voice.freq = 280.0f + 600.0f * rng.nextFloat();
            voice.freqEnd = voice.freq * (0.7f + 0.6f * rng.nextFloat());
            voice.chop = 18.0f + 60.0f * rng.nextFloat();   // Impulse pro Sekunde
            voice.holdCounter = 0;
            voice.gain = 1.4f;
            voice.sendDelay = 0.25f;
            voice.sendVerb = 0.5f;
            voice.bp.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            voice.bp.reset();
            voice.bp.setResonance (9.0f);
            break;

        case Sweeps:
        default:
            voice.freq = 2400.0f + 1800.0f * rng.nextFloat();
            voice.freqEnd = 120.0f + 180.0f * rng.nextFloat();
            voice.length = (int) (sr * (1.2 + 1.4 * rng.nextDouble()));
            voice.gain = 0.022f;
            voice.sendDelay = 0.3f;
            voice.sendVerb = 0.9f;
            break;
    }

    shown[(size_t) voice.type] = 1.0f;
    publishEvent (voice.type, voice.pan);
}

float PlutoEngine::renderVoice (Voice& v)
{
    const float t = (float) v.age / (float) sr;
    const float len = (float) v.length / (float) sr;
    float out = 0.0f;

    auto sine = [&v, this] (float f)
    {
        v.phase += f / sr;
        if (v.phase >= 1.0) v.phase -= 1.0;
        return (float) std::sin (v.phase * juce::MathConstants<double>::twoPi);
    };

    switch (v.type)
    {
        case Blips:
        {
            const int pos = v.age % v.repeatLen;
            const float tt = (float) pos / (float) sr;
            const float beep = 0.028f;
            const float env = tt < beep ? juce::jmin (1.0f, tt / 0.002f) * juce::jmin (1.0f, (beep - tt) / 0.004f) : 0.0f;
            out = sine (v.freq * ((v.age / v.repeatLen) % 2 == 1 ? 0.75f : 1.0f)) * env;
            break;
        }

        case Drips:
        {
            const float f = v.freqEnd + (v.freq - v.freqEnd) * std::exp (-t / 0.014f);
            out = sine (f) * juce::jmin (1.0f, t / 0.001f) * std::exp (-t / 0.045f);
            break;
        }

        case Pings:
        {
            const float vib = 1.0f + 0.003f * std::sin (juce::MathConstants<float>::twoPi * 5.0f * t);
            out = sine (v.freq * vib) * juce::jmin (1.0f, t / 0.003f) * std::exp (-t / 0.32f);
            break;
        }

        case Static:
        {
            const float env = juce::jmin (1.0f, t / 0.03f) * juce::jmin (1.0f, (len - t) / 0.08f);
            const float chop = std::sin (juce::MathConstants<float>::twoPi * v.chop * t) > -0.2f ? 1.0f : 0.25f;
            out = v.bp.processSample (0, noise()) * env * chop;
            break;
        }

        case Glitch:
        {
            const int pos = v.age % v.repeatLen;
            const int open = (v.repeatLen * 6) / 10;
            if (--v.holdCounter <= 0)
            {
                v.holdCounter = v.holdPeriod;
                v.holdValue = std::round (noise() * v.bitSteps) / v.bitSteps;
            }
            const float edge = juce::jmin (1.0f, (float) pos / 24.0f) * juce::jmin (1.0f, (float) (open - pos) / 24.0f);
            out = pos < open ? v.holdValue * edge : 0.0f;
            break;
        }

        case Morse:
        {
            const int unit = juce::jmin (63, v.age / v.repeatLen);
            const float target = ((v.pattern >> unit) & 1u) ? 1.0f : 0.0f;
            v.envS += (target - v.envS) * (1.0f - std::exp (-1.0f / (0.0015f * (float) sr)));
            out = sine (v.freq) * v.envS;
            break;
        }

        case Data:
        {
            if (--v.holdCounter <= 0)
            {
                v.holdCounter = v.holdPeriod;
                v.freq = 600.0f + 3400.0f * rng.nextFloat();
                v.holdValue = rng.nextFloat() < 0.15f ? 0.0f : 1.0f;   // kurze Lücken
            }
            const float edge = juce::jmin (1.0f, t / 0.003f) * juce::jmin (1.0f, (len - t) / 0.005f);
            out = (sine (v.freq) > 0.0f ? 1.0f : -1.0f) * v.holdValue * edge;
            break;
        }

        case Thunder:
        {
            const float attack = v.freq;
            const float env = t < attack ? t / attack : std::exp (-(t - attack) / (len * 0.3f));
            v.envS += 0.002f * (noise() - v.envS);                       // langsames Grollen
            const float roll = 0.5f + 0.5f * juce::jlimit (-1.0f, 1.0f, v.envS * 8.0f);
            out = v.bp.processSample (0, noise()) * env * roll;
            break;
        }

        case Swoosh:
        {
            const float k = t / len;
            if (v.age % 16 == 0)
                v.bp.setCutoffFrequency (juce::jmin (v.freq * std::pow (v.freqEnd / v.freq, k), (float) sr * 0.4f));
            const float env = k * k * k * juce::jmin (1.0f, (len - t) / 0.02f);
            out = v.bp.processSample (0, noise()) * env;
            break;
        }

        case Creak:
        {
            if (v.age % 16 == 0)
                v.bp.setCutoffFrequency (v.freq + (v.freqEnd - v.freq) * (t / len));
            float impulse = 0.0f;
            if (--v.holdCounter <= 0)
            {
                v.holdCounter = juce::jmax (1, (int) ((float) sr / v.chop * (0.6f + 0.8f * rng.nextFloat())));
                impulse = 0.5f + 0.5f * rng.nextFloat();
            }
            const float env = std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, t / len));
            out = v.bp.processSample (0, impulse) * env;
            break;
        }

        case Sweeps:
        default:
        {
            const float k = t / len;
            const float f = v.freq * std::pow (v.freqEnd / v.freq, k);
            const float env = std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, k));
            out = sine (f) * env * env;
            break;
        }
    }

    if (++v.age >= v.length)
        v.active = false;

    return out * v.gain;
}

void PlutoEngine::process (float* left, float* right, int numSamples, double bpm, double ppqAtStart, bool hostPlaying)
{
    // Szenenwechsel übernehmen
    const auto pending = pendingSeed.exchange (-1);
    if (pending >= 0)
    {
        scene = makeScene ((juce::uint32) pending);
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = scene.roomSize;
        rp.damping = 0.55f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        reverb.setParameters (rp);

        juce::uint32 mask = 0;
        for (int i = 0; i < numBeds; ++i) if (scene.bedGain[(size_t) i] > 0.0f) mask |= 1u << i;
        for (int i = 0; i < numEventTypes; ++i) if (scene.eventWeight[(size_t) i] > 0.0f) mask |= 1u << (numBeds + i);
        display.seed.store ((juce::uint32) pending);
        display.activeMask.store (mask);
    }

    rootNow = ((rootNote.load() % 12) + 12) % 12;
    scaleNow = juce::jlimit (0, numScales - 1, scaleIndex.load());

    bpm = juce::jlimit (40.0, 240.0, bpm > 0.0 ? bpm : 122.0);
    const double samplesPerBeat = sr * 60.0 / bpm;
    const double samplesPerStep = samplesPerBeat / 4.0;
    const float targetDelayL = (float) (samplesPerStep * scene.delayStepsL);
    const float targetDelayR = (float) (samplesPerStep * scene.delayStepsR);
    if (delayTimeL <= 0.0f) { delayTimeL = targetDelayL; delayTimeR = targetDelayR; }

    const int delaySize = delayBuf.getNumSamples();
    auto* dL = delayBuf.getWritePointer (0);
    auto* dR = delayBuf.getWritePointer (1);
    float* out[2] = { left, right };

    float rainPeak = 0.0f, geigerPeak = 0.0f, cricketPeak = 0.0f, bubblePeak = 0.0f;

    for (int offset = 0; offset < numSamples; )
    {
        const int n = juce::jmin (numSamples - offset, verbBuf.getNumSamples());
        verbBuf.clear (0, n);
        auto* vb = verbBuf.getArrayOfWritePointers();

        for (int i = 0; i < n; ++i)
        {
            const int s = offset + i;
            for (int b = 0; b < numBeds; ++b)
                bedLevel[(size_t) b] += bedSmoothing * (scene.bedGain[(size_t) b] - bedLevel[(size_t) b]);

            // --- Takt: 16tel-Raster vom Host oder intern
            bool newStep = false;
            int stepInBar = 0;
            if (hostPlaying)
            {
                const double ppq = ppqAtStart + (double) s / samplesPerBeat;
                const auto stepIndex = (juce::int64) std::floor (ppq * 4.0);
                if (stepIndex != lastPpqStep)
                {
                    newStep = lastPpqStep >= 0 || ppq * 4.0 - (double) stepIndex < 0.01;
                    lastPpqStep = stepIndex;
                }
                stepInBar = (int) (((stepIndex % 16) + 16) % 16);
            }
            else
            {
                lastPpqStep = -1;
                stepPhase += 1.0 / samplesPerStep;
                if (stepPhase >= 1.0) { stepPhase -= 1.0; newStep = true; }
            }

            if (newStep)
            {
                // Offbeats bevorzugt – Dub-Gefühl
                const float bias = (stepInBar % 4 == 2) ? 2.0f : 1.0f;
                if (rng.nextFloat() < scene.eventDensity * bias)
                    triggerEvent (samplesPerStep);
            }

            // --- Langsame Steuerung alle 32 Samples
            if (++controlCounter >= 32)
            {
                controlCounter = 0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int b = 0; b < 2; ++b)
                    {
                        if (rng.nextFloat() < 0.0025f)
                            windTarget[ch][b] = 220.0f * scene.windCentre * std::pow (2.0f, rng.nextFloat() * 3.4f);
                        windFreq[ch][b] += 0.004f * (windTarget[ch][b] - windFreq[ch][b]);
                        windBand[ch][b].setCutoffFrequency (juce::jmin (windFreq[ch][b], (float) sr * 0.4f));
                    }
                if (rng.nextFloat() < 0.003f) windSwellTarget = 0.25f + 0.75f * rng.nextFloat();
                if (rng.nextFloat() < 0.002f) rumbleSwellTarget = 0.3f + 0.7f * rng.nextFloat();
                if (rng.nextFloat() < 0.002f) swTarget = scaleFreq (4, 6);   // gleitet zwischen Tönen der Tonleiter
                if (rng.nextFloat() < 0.0015f) geigerBurstTarget = rng.nextFloat() < 0.25f ? 4.0f + 8.0f * rng.nextFloat() : 0.4f + rng.nextFloat();
                if (rng.nextFloat() < 0.0015f) swFadeTarget = rng.nextFloat() < 0.6f ? 1.0f : 0.0f;
                swFreq += 0.01f * (swTarget - swFreq);
                swNoise.setCutoffFrequency (juce::jmin (swFreq * 1.5f, (float) sr * 0.4f));

                const float k = 0.0005f;
                feedbackNow += k * 20.0f * (scene.feedback - feedbackNow);
                verbNow += k * 20.0f * (scene.verbSend - verbNow);
                widthNow += k * 20.0f * (scene.width - widthNow);
                toneNow += k * 20.0f * (scene.tone - toneNow);
                toneLP[0].setLowpass (toneNow, sr);
                toneLP[1].setLowpass (toneNow, sr);
                for (int ch = 0; ch < 2; ++ch)
                {
                    rumbleLP[ch][0].setLowpass (scene.rumbleFreq, sr);
                    rumbleLP[ch][1].setLowpass (scene.rumbleFreq, sr);
                }
            }
            windSwell += 0.00002f * (windSwellTarget - windSwell);
            rumbleSwell += 0.00002f * (rumbleSwellTarget - rumbleSwell);
            swFade += 0.00003f * (swFadeTarget - swFade);
            geigerBurst += 0.0001f * (geigerBurstTarget - geigerBurst);

            float atmo[2] {}, verbIn[2] {}, sendD[2] {};

            // --- Vinyl
            const float vinyl = bedLevel[Vinyl];
            recordPhase += 1.0 / (sr * 1.8);   // 33 1/3 U/min
            if (recordPhase >= 1.0) recordPhase -= 1.0;
            const float wobble = 1.0f + 0.6f * std::sin ((float) recordPhase * juce::MathConstants<float>::twoPi);
            if (vinyl > 0.0001f)
            {
                if (rng.nextFloat() < (0.1f + scene.crackleRate * 0.015f) / (float) sr)
                {
                    const float a = (0.1f + 0.2f * rng.nextFloat()) * vinyl;
                    const float spread = rng.nextFloat() * 0.4f;
                    popEnv[0] = a * (1.0f - spread);
                    popEnv[1] = a * (0.6f + spread);
                }
                for (int ch = 0; ch < 2; ++ch)
                {
                    if (rng.nextFloat() < scene.crackleRate * wobble * 0.5f / (float) sr)
                    {
                        const float r = rng.nextFloat();
                        crackleEnv[ch] = (r * r * r * 0.3f + 0.025f) * vinyl;
                        crackleSign[ch] = rng.nextBool() ? 1.0f : -1.0f;
                    }
                    atmo[ch] += crackleHP[ch].hp (crackleEnv[ch] * crackleSign[ch] * (0.6f + 0.4f * noise()));
                    atmo[ch] += popLP[ch].lp (popEnv[ch] * (rng.nextBool() ? 1.0f : 0.7f));
                    crackleEnv[ch] *= crackleDecay;
                    popEnv[ch] *= popDecay;
                }
            }

            // --- Rauschen
            if (bedLevel[Hiss] > 0.0001f)
                for (int ch = 0; ch < 2; ++ch)
                    atmo[ch] += hissLP[ch].lp (hissHP[ch].hp (noise())) * 0.016f * bedLevel[Hiss] * (0.85f + 0.15f * wobble);

            // --- Sonnenwind
            if (bedLevel[SolarWind] > 0.0001f)
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float p = pink (ch);
                    const float w = (windBand[ch][0].processSample (0, p) + 0.7f * windBand[ch][1].processSample (0, p))
                                    * 0.1f * bedLevel[SolarWind] * windSwell;
                    atmo[ch] += w;
                    verbIn[ch] += w * 0.25f;
                }

            // --- Regen: viele winzige Tropfen auf weichem Rauschen
            if (bedLevel[Rain] > 0.0001f)
            {
                if (rng.nextFloat() < scene.rainRate / (float) sr)
                    for (auto& dr : drops)
                        if (dr.env < 0.0005f)
                        {
                            const float r = rng.nextFloat();
                            dr.phase = 0.0f;
                            dr.inc = (1800.0f + 4200.0f * rng.nextFloat()) / (float) sr;
                            dr.env = (0.015f + 0.09f * r * r) * bedLevel[Rain];
                            dr.decay = decayCoeff (0.002f + 0.004f * rng.nextFloat(), sr);
                            dr.pan = rng.nextFloat();
                            break;
                        }
                for (auto& dr : drops)
                {
                    if (dr.env < 0.0005f) continue;
                    dr.phase += dr.inc;
                    if (dr.phase >= 1.0f) dr.phase -= 1.0f;
                    const float x = std::sin (dr.phase * juce::MathConstants<float>::twoPi) * dr.env;
                    dr.env *= dr.decay;
                    atmo[0] += x * std::sqrt (1.0f - dr.pan);
                    atmo[1] += x * std::sqrt (dr.pan);
                    rainPeak = juce::jmax (rainPeak, dr.env);
                }
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float bed = rainLP[ch].lp (noise()) * 0.02f * bedLevel[Rain];
                    atmo[ch] += bed;
                    verbIn[ch] += bed * 0.5f;
                }
            }

            // --- Rumpeln: fernes Raumschiff
            if (bedLevel[Rumble] > 0.0001f)
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float x = rumbleHP[ch].hp (rumbleLP[ch][1].lp (rumbleLP[ch][0].lp (pink (ch))));
                    atmo[ch] += x * 0.5f * bedLevel[Rumble] * rumbleSwell;
                }

            // --- Kurzwelle: wandernder Pfeifton und Funkrauschen
            if (bedLevel[Shortwave] > 0.0001f)
            {
                swPhase += swFreq / sr;
                if (swPhase >= 1.0) swPhase -= 1.0;
                const float tone = (float) std::sin (swPhase * juce::MathConstants<double>::twoPi) * 0.006f * swFade;
                const float hiss = swNoise.processSample (0, noise()) * 0.03f * (0.4f + 0.6f * wobble * 0.6f);
                const float x = (tone + hiss) * bedLevel[Shortwave];
                atmo[0] += x * 0.8f;
                atmo[1] += x * 0.6f;
                verbIn[0] += x * 0.4f;
                verbIn[1] += x * 0.4f;
            }

            // --- Geigerzähler: Knacken mit unruhiger Rate und plötzlichen Schauern
            if (bedLevel[Geiger] > 0.0001f)
            {
                if (rng.nextFloat() < scene.geigerRate * geigerBurst / (float) sr)
                {
                    geigerEnv = (0.18f + 0.15f * rng.nextFloat()) * bedLevel[Geiger];
                    geigerRingEnv = geigerEnv * 0.25f;
                    geigerPan = 0.35f + 0.3f * rng.nextFloat();
                    geigerPeak = 1.0f;
                }
                geigerRingPhase += 3300.0 / sr;
                if (geigerRingPhase >= 1.0) geigerRingPhase -= 1.0;
                const float x = geigerHP.hp (geigerEnv * (rng.nextBool() ? 1.0f : -0.6f))
                              + geigerRingEnv * (float) std::sin (geigerRingPhase * juce::MathConstants<double>::twoPi);
                geigerEnv *= geigerDecay;
                geigerRingEnv *= geigerRingDecay;
                atmo[0] += x * std::sqrt (1.0f - geigerPan);
                atmo[1] += x * std::sqrt (geigerPan);
            }

            // --- Grillen: kurze Zirp-Pulse, jede Grille mit eigenem Rhythmus
            if (bedLevel[Crickets] > 0.0001f)
                for (auto& c : crickets)
                {
                    if (++c.pos >= c.period)
                    {
                        c.pos = 0;
                        c.period = (int) (sr * (0.45 + 0.6 * rng.nextDouble()));
                        c.pulses = 3 + rng.nextInt (3);
                        c.on = rng.nextFloat() < 0.75f;
                        c.freq = scene.cricketFreq * (0.95f + 0.1f * rng.nextFloat());
                    }
                    const int pulseLen = (int) (sr * 0.016), gap = (int) (sr * 0.011);
                    const int idx = c.pos / (pulseLen + gap), inPulse = c.pos % (pulseLen + gap);
                    if (! c.on || idx >= c.pulses || inPulse >= pulseLen) continue;
                    c.phase += c.freq / (float) sr;
                    if (c.phase >= 1.0f) c.phase -= 1.0f;
                    const float env = std::sin (juce::MathConstants<float>::pi * (float) inPulse / (float) pulseLen);
                    const float x = std::sin (c.phase * juce::MathConstants<float>::twoPi) * env * 0.03f * bedLevel[Crickets];
                    atmo[0] += x * std::sqrt (1.0f - c.pan);
                    atmo[1] += x * std::sqrt (c.pan);
                    verbIn[0] += x * 0.3f;
                    verbIn[1] += x * 0.3f;
                    cricketPeak = 1.0f;
                }

            // --- Blubbern: kurze, nach oben gleitende Blasen
            if (bedLevel[Bubbles] > 0.0001f)
            {
                if (rng.nextFloat() < scene.bubbleRate / (float) sr)
                    for (auto& b : bubbles)
                        if (b.env < 0.0005f)
                        {
                            const float life = 0.02f + 0.04f * rng.nextFloat();
                            b.phase = 0.0f;
                            b.inc = (300.0f + 900.0f * rng.nextFloat()) / (float) sr;
                            b.glide = std::pow (1.8f, 1.0f / (life * (float) sr));
                            b.env = (0.02f + 0.04f * rng.nextFloat()) * bedLevel[Bubbles];
                            b.decay = decayCoeff (life * 0.5f, sr);
                            b.pan = 0.2f + 0.6f * rng.nextFloat();
                            break;
                        }
                for (auto& b : bubbles)
                {
                    if (b.env < 0.0005f) continue;
                    b.phase += b.inc;
                    b.inc *= b.glide;
                    if (b.phase >= 1.0f) b.phase -= 1.0f;
                    const float x = std::sin (b.phase * juce::MathConstants<float>::twoPi) * b.env;
                    b.env *= b.decay;
                    atmo[0] += x * std::sqrt (1.0f - b.pan);
                    atmo[1] += x * std::sqrt (b.pan);
                    verbIn[0] += x * 0.3f;
                    verbIn[1] += x * 0.3f;
                    bubblePeak = juce::jmax (bubblePeak, b.env * 20.0f);
                }
            }

            // --- Ereignisse
            for (auto& v : voices)
            {
                if (! v.active) continue;
                const float x = renderVoice (v);
                const float gl = std::sqrt (1.0f - v.pan), gr = std::sqrt (v.pan);
                atmo[0] += x * gl;
                atmo[1] += x * gr;
                sendD[0] += x * gl * v.sendDelay;
                sendD[1] += x * gr * v.sendDelay;
                verbIn[0] += x * gl * v.sendVerb;
                verbIn[1] += x * gr * v.sendVerb;
            }

            // --- Dub-Delay (Ping-Pong, gefilterte Rückkopplung)
            delayTimeL += 0.0005f * (targetDelayL - delayTimeL);
            delayTimeR += 0.0005f * (targetDelayR - delayTimeR);
            auto readDelay = [&] (const float* buf, float time)
            {
                float pos = (float) delayWrite - time;
                while (pos < 0.0f) pos += (float) delaySize;
                const int i0 = (int) pos;
                const int i1 = (i0 + 1) % delaySize;
                const float frac = pos - (float) i0;
                return buf[i0] + frac * (buf[i1] - buf[i0]);
            };
            const float echoL = readDelay (dL, delayTimeL);
            const float echoR = readDelay (dR, delayTimeR);
            dL[delayWrite] = sendD[0] + feedbackNow * std::tanh (delayLP[0].lp (delayHP[0].hp (echoR)) * 1.4f) / 1.4f;
            dR[delayWrite] = sendD[1] + feedbackNow * std::tanh (delayLP[1].lp (delayHP[1].hp (echoL)) * 1.4f) / 1.4f;
            delayWrite = (delayWrite + 1) % delaySize;

            atmo[0] += echoL * 0.8f;
            atmo[1] += echoR * 0.8f;
            vb[0][i] = verbIn[0] + echoL * 0.3f;
            vb[1][i] = verbIn[1] + echoR * 0.3f;

            // Klangfarbe und Stereobreite der Szene
            const float l = toneLP[0].lp (outHP[0].hp (atmo[0]));
            const float r = toneLP[1].lp (outHP[1].hp (atmo[1]));
            const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * widthNow;
            out[0][s] += mid + side;
            out[1][s] += mid - side;
        }

        // --- Hall (blockweise)
        juce::dsp::AudioBlock<float> block (vb, 2, (size_t) n);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (block));
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::addWithMultiply (out[ch] + offset, vb[ch], verbNow, n);

        offset += n;
    }

    // --- Anzeige
    const float blockDecay = std::exp (-(float) numSamples / (float) (sr * 0.35));
    for (auto& v : shown) v *= blockDecay;
    float crackle = 0.0f;
    for (int ch = 0; ch < 2; ++ch) crackle = juce::jmax (crackle, (crackleEnv[ch] + popEnv[ch]) * 4.0f);
    shown[Vinyl]     = juce::jmax (shown[Vinyl], bedLevel[Vinyl] * 0.5f + juce::jmin (0.5f, crackle));
    shown[Hiss]      = bedLevel[Hiss] * (0.75f + 0.25f * (float) std::sin (recordPhase * juce::MathConstants<double>::twoPi));
    shown[SolarWind] = bedLevel[SolarWind] * (0.3f + 0.7f * windSwell);
    shown[Rain]      = juce::jmax (shown[Rain], bedLevel[Rain] * 0.4f + juce::jmin (0.6f, rainPeak * 8.0f));
    shown[Rumble]    = bedLevel[Rumble] * (0.3f + 0.7f * rumbleSwell);
    shown[Shortwave] = bedLevel[Shortwave] * (0.35f + 0.65f * swFade);
    shown[Geiger]    = juce::jmax (shown[Geiger], bedLevel[Geiger] * (0.3f + 0.7f * geigerPeak));
    shown[Crickets]  = juce::jmax (shown[Crickets], bedLevel[Crickets] * (0.3f + 0.7f * cricketPeak));
    shown[Bubbles]   = juce::jmax (shown[Bubbles], bedLevel[Bubbles] * (0.3f + 0.7f * juce::jmin (1.0f, bubblePeak)));
    for (int i = 0; i < numElements; ++i)
        display.activity[(size_t) i].store (juce::jlimit (0.0f, 1.0f, shown[(size_t) i]));
}

#if PLUTO_TESTS
void PlutoEngine::soloForTest (int element)
{
    pendingSeed.store (-1);
    scene = makeScene (1);
    scene.bedGain = {};
    scene.eventWeight = {};
    scene.eventDensity = 0.0f;
    if (element < numBeds)
        scene.bedGain[(size_t) element] = 1.0f;
    else
    {
        scene.eventWeight[(size_t) (element - numBeds)] = 1.0f;
        scene.eventDensity = 0.1f;
    }
    bedLevel = scene.bedGain;
}
#endif
