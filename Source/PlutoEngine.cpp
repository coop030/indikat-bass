#include "PlutoEngine.h"

namespace
{
    float decayCoeff (float seconds, double sr) { return std::exp (-1.0f / (seconds * (float) sr)); }

    // Hohe Töne aus A-Moll-Pentatonik, damit Blips nicht gegen den Track arbeiten
    constexpr float blipNotes[] = { 1318.51f, 1567.98f, 1760.0f, 2093.0f, 2349.32f, 2637.02f, 3135.96f };
    constexpr float pingNotes[] = { 440.0f, 523.25f, 587.33f, 659.25f, 783.99f, 880.0f };

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
    static const char* names[] = { "VINYL", "HISS", "SOLAR WIND", "RAIN", "RUMBLE", "SHORTWAVE",
                                   "BLIPS", "DRIPS", "PINGS", "STATIC", "GLITCH", "SWEEPS" };
    return juce::isPositiveAndBelow (e, (int) numElements) ? names[e] : "";
}

PlutoEngine::Scene PlutoEngine::makeScene (juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed);
    Scene s;

    // 1–3 Flächen, Vinyl am wahrscheinlichsten
    const int numBedsChosen = 1 + (r.nextFloat() < 0.75f ? 1 : 0) + (r.nextFloat() < 0.35f ? 1 : 0);
    const auto beds = pickDistinct<numBeds> (r, { 3.0f, 1.4f, 2.0f, 1.5f, 1.2f, 1.0f }, numBedsChosen);
    for (int i = 0; i < numBeds; ++i)
        s.bedGain[(size_t) i] = beds[(size_t) i] ? 0.55f + 0.45f * r.nextFloat() : 0.0f;

    // 1–3 Ereignis-Typen
    const int numEventsChosen = 1 + r.nextInt (3);
    const auto events = pickDistinct<numEventTypes> (r, { 2.0f, 2.0f, 1.5f, 1.0f, 1.5f, 1.0f }, numEventsChosen);
    for (int i = 0; i < numEventTypes; ++i)
        s.eventWeight[(size_t) i] = events[(size_t) i] ? 0.4f + 0.6f * r.nextFloat() : 0.0f;
    s.eventDensity = 0.02f + 0.07f * std::pow (r.nextFloat(), 1.4f);

    s.crackleRate = 8.0f + 32.0f * r.nextFloat();
    s.rainRate = 18.0f + 70.0f * r.nextFloat();
    s.windCentre = 0.6f + r.nextFloat();
    s.rumbleFreq = 70.0f + 90.0f * r.nextFloat();
    s.shortwaveBase = 500.0f * std::pow (2.0f, r.nextFloat() * 2.0f);
    s.blipRegister = r.nextFloat() < 0.3f ? 0.5f : 1.0f;

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
            voice.freq = blipNotes[rng.nextInt ((int) std::size (blipNotes))] * scene.blipRegister;
            voice.repeatLen = juce::jmax (1, step / 2);
            voice.length = (1 + rng.nextInt (3)) * voice.repeatLen;
            voice.gain = 0.045f;
            voice.sendDelay = 0.75f;
            voice.sendVerb = 0.3f;
            break;

        case Drips:
            voice.freq = 850.0f + 1000.0f * rng.nextFloat();
            voice.freqEnd = voice.freq * 2.3f;
            voice.length = (int) (sr * 0.25);
            voice.gain = 0.09f;
            voice.sendDelay = 0.85f;
            voice.sendVerb = 0.35f;
            break;

        case Pings:
            voice.freq = pingNotes[rng.nextInt ((int) std::size (pingNotes))] * (rng.nextBool() ? 1.0f : 0.5f);
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

    float rainPeak = 0.0f;

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
                if (rng.nextFloat() < 0.002f) swTarget = scene.shortwaveBase * std::pow (2.0f, rng.nextFloat() * 1.5f - 0.5f);
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
    for (int i = 0; i < numElements; ++i)
        display.activity[(size_t) i].store (juce::jlimit (0.0f, 1.0f, shown[(size_t) i]));
}
