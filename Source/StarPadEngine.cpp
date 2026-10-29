#include "StarPadEngine.h"

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    const char* const noteNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    struct Quality { int iv[6]; int count; const char* label; };
    const Quality qM7   { { 0, 3, 7, 10 }, 4, "m7" };
    const Quality qM9   { { 0, 3, 7, 10, 14 }, 5, "m9" };
    const Quality qM11  { { 0, 3, 10, 14, 17 }, 5, "m11" };
    const Quality qMaj7 { { 0, 4, 7, 11 }, 4, "maj7" };
    const Quality qMaj9 { { 0, 4, 7, 11, 14 }, 5, "maj9" };
    const Quality qSus2 { { 0, 2, 7, 12 }, 4, "sus2" };
    const Quality qDub  { { 0, 7, 10, 15 }, 4, "m7" };     // Dub-Stab: Moll-7 mit Terz oben
    const Quality q5    { { 0, 7, 12, 19 }, 4, "5" };
    const Quality q5a9  { { 0, 7, 14, 19 }, 4, "5add9" };
    const Quality qSus4 { { 0, 5, 7, 12 }, 4, "sus4" };

    struct PadDef { int offset; const Quality* q; const char* degree; };
    const PadDef padSets[3][StarPadEngine::numPads] = {
        { { 0, &qM9, "i" }, { 5, &qM9, "iv" }, { 8, &qMaj9, "VI" }, { 7, &qM7, "v" },
          { 3, &qMaj7, "III" }, { 10, &qSus2, "VII" }, { 0, &qM11, "i" }, { -2, &qMaj9, "VII" } },
        { { 0, &qDub, "i" }, { 5, &qDub, "iv" }, { 3, &qDub, "III" }, { 7, &qDub, "v" },
          { 10, &qDub, "VII" }, { -4, &qMaj7, "VI" }, { 2, &qDub, "ii" }, { -2, &qDub, "VII" } },
        { { 0, &q5, "I" }, { 5, &q5, "IV" }, { 7, &q5a9, "V" }, { 3, &q5, "III" },
          { 10, &q5a9, "VII" }, { 0, &qSus4, "I" }, { -4, &q5a9, "VI" }, { -2, &q5, "VII" } },
    };
    // Qualität, wenn eine einzelne MIDI-Note einen Akkord auslöst
    const Quality* const midiChord[3] { &qM9, &qDub, &q5 };

    // 16tel-Raster: welche Schritte einen Stab spielen
    const bool pulsePatterns[5][16] = {
        {},
        { 0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0 },   // Offbeat
        { 1,0,0,1, 0,0,1,0, 0,0,1,0, 0,1,0,0 },   // Dub  x··x··x···x··x··
        { 0,0,0,1, 0,0,1,0, 0,0,0,1, 0,0,1,0 },   // Synkope
        { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0 },   // Achtel
    };

    const double lfoBeats[] { 4.0, 8.0, 16.0, 32.0, 64.0, 128.0 };
    const float delayBeats[] { 0.5f, 0.75f, 1.0f, 1.5f };

    int applyQuality (const Quality& q, int root, bool wide, int* out)
    {
        for (int i = 0; i < q.count; ++i) out[i] = root + q.iv[i];
        if (wide && q.count >= 4)
        {
            out[1] += 12;
            std::sort (out, out + q.count);
        }
        return q.count;
    }

    inline float polyBlep (float t, float dt)
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    inline float mtof (float m) { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }
}

const juce::StringArray StarPadEngine::waveNames     { "Saw", "Square", "Triangle", "Sine" };
const juce::StringArray StarPadEngine::chordSetNames { "Deep", "Dub", "Fifths" };
const juce::StringArray StarPadEngine::voicingNames  { "Wide", "Close" };
const juce::StringArray StarPadEngine::pulseNames    { "Pad", "Offbeat", "Dub", "Syncopated", "Eighths" };
const juce::StringArray StarPadEngine::lfoRateNames  { "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS", "32 BARS" };
const juce::StringArray StarPadEngine::delayDivNames { "1/8", "1/8 D", "1/4", "1/4 D" };
const juce::StringArray StarPadEngine::keyNames      { "C minor", "C# minor", "D minor", "D# minor", "E minor", "F minor",
                                                       "F# minor", "G minor", "G# minor", "A minor", "A# minor", "B minor" };

juce::String StarPadEngine::padLabel (int chordSet, int key, int pad)
{
    const auto& d = padSets[juce::jlimit (0, 2, chordSet)][pad];
    return juce::String (noteNames[((key + d.offset) % 12 + 12) % 12]) + d.q->label;
}

juce::String StarPadEngine::padDegree (int chordSet, int pad)
{
    return padSets[juce::jlimit (0, 2, chordSet)][pad].degree;
}

int StarPadEngine::padChord (const StarPadParams& p, int pad, int* notes, int& root)
{
    const auto& d = padSets[juce::jlimit (0, 2, p.chordSet)][juce::jlimit (0, numPads - 1, pad)];
    int base = 48 + p.key;
    if (p.key >= 6) base -= 12;          // Grundtöne zwischen F#2 und F3
    root = base + p.octave * 12 + d.offset;
    return applyQuality (*d.q, root, p.voicing == 0, notes);
}

//==============================================================================
void StarPadEngine::prepare (double sampleRate)
{
    sr = sampleRate;
    const juce::dsp::ProcessSpec stereo { sampleRate, 512, 2 };

    filt1.prepare (stereo); filt1.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filt2.prepare (stereo); filt2.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filt2.setResonance (0.707f);

    fbLow.prepare (stereo);  fbLow.setType (juce::dsp::StateVariableTPTFilterType::lowpass);  fbLow.setCutoffFrequency (2600.0f);
    fbHigh.prepare (stereo); fbHigh.setType (juce::dsp::StateVariableTPTFilterType::highpass); fbHigh.setCutoffFrequency (280.0f);

    sparkHP.prepare (stereo); sparkHP.setType (juce::dsp::StateVariableTPTFilterType::highpass); sparkHP.setCutoffFrequency (900.0f);
    noiseBP.prepare (stereo); noiseBP.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    noiseBP.setCutoffFrequency (2200.0f); noiseBP.setResonance (0.5f);

    for (auto& b : chorusBuf) b.assign ((size_t) (sampleRate * 0.05) + 8, 0.0f);
    for (auto& b : delayBuf)  b.assign ((size_t) (sampleRate * 4.5) + 8, 0.0f);
    chorusPos = delayPos = 0;

    reverb.prepare (sampleRate);

    for (auto& v : voices) v.active = false;
    for (auto& s : sparks) s.active = false;

    delaySamples.reset (sampleRate, 0.25);
    delaySamples.setCurrentAndTargetValue ((float) (sampleRate * 60.0 / 122.0 * delayBeats[p.delayDiv]));
    for (auto* s : { &chorusS, &fbS, &delayMixS, &revMixS, &noiseS, &outS })
        s->reset (sampleRate, 0.05);
    chorusS.setCurrentAndTargetValue (p.chorus);
    fbS.setCurrentAndTargetValue (p.feedback);
    delayMixS.setCurrentAndTargetValue (p.delayMix);
    revMixS.setCurrentAndTargetValue (p.revMix);
    noiseS.setCurrentAndTargetValue (p.noise);
    outS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.outGain));

    lastStep = std::numeric_limits<juce::int64>::min();
    prepared = true;
}

void StarPadEngine::setParams (const StarPadParams& np)
{
    const StarPadParams old = p;
    p = np;

    // Laufende Stimmen folgen Wellenform und Verstimmung sofort
    if (old.wave != p.wave || old.detune != p.detune)
        for (auto& v : voices)
            if (v.active && ! v.isSub)
            {
                v.wave = p.wave;
                refreshIncrements (v);
            }

    if (old.pulse == 0 && p.pulse != 0)
        releaseAllHeld();                           // Pad -> Stabs
    else if (old.pulse != 0 && p.pulse == 0)
    {
        const int n = collectSources (sourceScratch);  // Stabs -> Pad: gehaltene Töne wieder als Fläche
        for (int i = 0; i < n; ++i) startSource (sourceScratch[i], -1);
    }
    else if (p.pulse == 0 && currentPad >= 0
             && (old.chordSet != p.chordSet || old.voicing != p.voicing || old.key != p.key
                 || old.octave != p.octave || old.voices != p.voices || (old.sub > 0) != (p.sub > 0)))
    {
        releaseTag (padTag);                        // gehaltenen Pad-Akkord neu aufbauen
        Source s;
        buildSource (padTag, 0, 1.0f, s);
        startSource (s, -1);
    }

    if (old.chordSet != p.chordSet || old.voicing != p.voicing || old.key != p.key || old.octave != p.octave
        || old.chordOn != p.chordOn)
        updateHeldMask();

    chorusS.setTargetValue (p.chorus);
    fbS.setTargetValue (p.feedback);
    delayMixS.setTargetValue (p.delayMix);
    revMixS.setTargetValue (p.revMix);
    noiseS.setTargetValue (p.noise);
    outS.setTargetValue (juce::Decibels::decibelsToGain (p.outGain));
    reverb.setParameters (p.revSize, 0.45f, true);
    filt1.setResonance (0.5f + p.reso * p.reso * 11.5f);
}

//==============================================================================
void StarPadEngine::buildSource (int tag, int midiNote, float velocity, Source& s) const
{
    s.tag = tag;
    const float vel = 0.6f + 0.4f * velocity;
    if (tag == padTag)
    {
        s.count = padChord (p, currentPad, s.notes, s.root);
        s.peak = 0.36f / std::sqrt ((float) s.count);
        s.subScale = 1.0f;
    }
    else if (p.chordOn)
    {
        s.root = midiNote + p.octave * 12;
        s.count = applyQuality (*midiChord[juce::jlimit (0, 2, p.chordSet)], s.root, p.voicing == 0, s.notes);
        s.peak = 0.36f / std::sqrt ((float) s.count) * vel;
        s.subScale = vel;
    }
    else
    {
        s.root = midiNote + p.octave * 12;
        s.notes[0] = s.root;
        s.count = 1;
        s.peak = 0.22f * vel;
        s.subScale = 0.5f * vel;   // gespielte Akkorde: Sub pro Ton leiser
    }
}

int StarPadEngine::collectSources (Source* out) const
{
    int n = 0;
    for (int note = 0; note < 128; ++note)
        if (midiHeld[note])
            buildSource (note, note, midiVel[note], out[n++]);
    if (currentPad >= 0)
        buildSource (padTag, 0, 1.0f, out[n++]);
    return n;
}

void StarPadEngine::updateHeldMask()
{
    juce::uint64 m[2] { 0, 0 };
    heldCount = 0;
    const int n = collectSources (sourceScratch);
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < sourceScratch[i].count; ++k)
        {
            const int note = juce::jlimit (0, 127, sourceScratch[i].notes[k]);
            if ((m[note >> 6] & (1ull << (note & 63))) == 0)
            {
                m[note >> 6] |= 1ull << (note & 63);
                heldNotes[heldCount++] = note;
            }
        }
    heldMask[0].store (m[0]);
    heldMask[1].store (m[1]);
}

StarPadEngine::Voice& StarPadEngine::allocVoice()
{
    for (auto& v : voices)
        if (! v.active) return v;

    // Stehlen: leiseste Stimme im Release, sonst die älteste
    Voice* best = nullptr;
    for (auto& v : voices)
        if (v.stage == 2 && (best == nullptr || v.env < best->env)) best = &v;
    if (best == nullptr)
        for (auto& v : voices)
            if (best == nullptr || v.age < best->age) best = &v;
    return *best;
}

void StarPadEngine::refreshIncrements (Voice& v) const
{
    for (int o = 0; o < v.nOsc; ++o)
        v.inc[o] = v.freq * std::pow (2.0f, (v.spread[o] * p.detune + v.jitter[o]) / 1200.0f) / (float) sr;
}

void StarPadEngine::startVoice (int note, float peak, int tag, int gateSamples, bool sub)
{
    auto& v = allocVoice();
    v = Voice();
    v.active = true;
    v.isSub = sub;
    v.tag = tag;
    v.age = ++ageCounter;
    v.freq = mtof ((float) note);
    v.wave = sub ? 3 : p.wave;
    v.nOsc = sub ? 1 : juce::jlimit (1, maxOsc, p.voices);

    for (int o = 0; o < v.nOsc; ++o)
    {
        v.spread[o] = v.nOsc == 1 ? 0.0f : (float) o / (float) (v.nOsc - 1) * 2.0f - 1.0f;
        v.jitter[o] = sub ? 0.0f : (rng.nextFloat() - 0.5f) * 4.0f;
        v.phase[o] = sub ? 0.0f : rng.nextFloat();
        const float pan = v.spread[o] * 0.75f;
        const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        v.gainL[o] = std::cos (a);
        v.gainR[o] = std::sin (a);
    }
    refreshIncrements (v);

    v.peak = sub ? peak : peak / std::sqrt ((float) v.nOsc);
    const float attack = sub ? juce::jmax (0.005f, p.attack) : p.attack;
    v.attackStep = v.peak / juce::jmax (1.0f, attack * (float) sr);
    v.relCoef = std::exp (-1.0f / (juce::jmax (0.01f, p.release / 4.0f) * (float) sr));
    v.gateLeft = gateSamples;
}

void StarPadEngine::startSource (const Source& s, int gateSamples)
{
    const int tag = gateSamples > 0 ? stabTag : s.tag;
    for (int i = 0; i < s.count; ++i)
        startVoice (s.notes[i], s.peak, tag, gateSamples, false);
    if (p.sub > 0.0f)
        startVoice (s.root - 12, p.sub * 0.28f * s.subScale, tag, gateSamples, true);
}

void StarPadEngine::releaseTag (int tag)
{
    for (auto& v : voices)
        if (v.active && v.tag == tag && v.stage != 2) v.stage = 2;
}

void StarPadEngine::releaseAllHeld()
{
    for (auto& v : voices)
        if (v.active && v.tag >= 0 && v.stage != 2) v.stage = 2;
}

void StarPadEngine::noteOn (int note, float velocity)
{
    if (midiHeld[note]) releaseTag (note);
    midiHeld[note] = true;
    midiVel[note] = velocity;
    if (p.pulse == 0)
    {
        Source s;
        buildSource (note, note, velocity, s);
        startSource (s, -1);
    }
    updateHeldMask();
}

void StarPadEngine::noteOff (int note)
{
    midiHeld[note] = false;
    releaseTag (note);
    updateHeldMask();
}

void StarPadEngine::setPad (int pad)
{
    if (pad == currentPad) return;
    releaseTag (padTag);
    currentPad = pad;
    if (pad >= 0 && p.pulse == 0)
    {
        Source s;
        buildSource (padTag, 0, 1.0f, s);
        startSource (s, -1);
    }
    updateHeldMask();
}

void StarPadEngine::allNotesOff()
{
    std::fill (std::begin (midiHeld), std::end (midiHeld), false);
    currentPad = -1;
    releaseAllHeld();
    updateHeldMask();
}

//==============================================================================
void StarPadEngine::spawnSpark()
{
    if (heldCount == 0) return;
    auto* s = std::find_if (std::begin (sparks), std::end (sparks), [] (const SparkVoice& v) { return ! v.active; });
    if (s == std::end (sparks)) return;

    const int pc = heldNotes[rng.nextInt (heldCount)] % 12;
    const bool fifth = rng.nextFloat() < 0.2f;
    const float m = 72.0f + (float) pc + 12.0f * (float) rng.nextInt (juce::jlimit (1, 3, p.starHeight)) + (fifth ? 7.0f : 0.0f);
    const float dur = 0.25f + rng.nextFloat() * 1.4f;
    const float lvl = p.starLevel * (0.04f + rng.nextFloat() * 0.08f);
    if (lvl <= 0.0001f) return;

    *s = SparkVoice();
    s->active = true;
    s->f = mtof (m);
    s->index = s->f * 1.2f;
    s->indexCoef = std::exp (std::log (1.0f / s->index) / (dur * 0.6f * (float) sr));
    s->peak = lvl;
    s->attackStep = lvl / (0.004f * (float) sr);
    s->decayCoef = std::exp (std::log (0.0001f / lvl) / (dur * (float) sr));
    const float pan = rng.nextFloat() * 1.6f - 0.8f;
    const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    s->gL = std::cos (a);
    s->gR = std::sin (a);

    int start1, size1, start2, size2;
    sparkFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0) sparkBuffer[start1] = { m, juce::jmin (1.4f, 0.4f + lvl * 9.0f), fifth };
    sparkFifo.finishedWrite (size1);
}

float StarPadEngine::readLine (const std::vector<float>& buf, int writePos, float delay) const
{
    const int size = (int) buf.size();
    float rp = (float) writePos - delay;
    while (rp < 0.0f) rp += (float) size;
    const int i0 = (int) rp;
    const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
    const float frac = rp - (float) i0;
    return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
}

void StarPadEngine::process (float* left, float* right, int n, double bpm, double ppq)
{
    if (! prepared) return;
    bpm = juce::jlimit (40.0, 300.0, bpm);
    const double ppqPerSample = bpm / 60.0 / sr;
    const float delayTarget = (float) (sr * 60.0 / bpm * delayBeats[juce::jlimit (0, 3, p.delayDiv)]);
    delaySamples.setTargetValue (juce::jmin (delayTarget, (float) delayBuf[0].size() - 16.0f));

    const float sparkChance = p.stars * 0.14f / (0.025f * (float) sr) * 32.0f;   // pro 32 Samples
    const auto* pattern = pulsePatterns[juce::jlimit (0, 4, p.pulse)];
    const int gateSamples = juce::jmax (16, (int) (p.gate * (float) sr));
    const float chorusInc[2] { twoPi * 0.23f / (float) sr, twoPi * 0.32f / (float) sr };
    float peak = 0.0f;
    int sp = scopePos.load();

    for (int i = 0; i < n; ++i)
    {
        const double pos = ppq + (double) i * ppqPerSample;

        // Puls: Stabs im 16tel-Raster des Hosts
        const auto step = (juce::int64) std::floor (pos * 4.0 + 1e-9);
        if (step != lastStep)
        {
            lastStep = step;
            if (p.pulse > 0 && pattern[(int) (((step % 16) + 16) % 16)])
            {
                const float accent = (step % 4 == 0) ? 1.0f : 0.85f + rng.nextFloat() * 0.1f;
                const int ns = collectSources (sourceScratch);
                for (int k = 0; k < ns; ++k)
                {
                    auto s = sourceScratch[k];
                    s.peak *= (0.4f / 0.36f) * accent;
                    startSource (s, gateSamples);
                }
                if (ns > 0)
                {
                    int start1, size1, start2, size2;
                    sparkFifo.prepareToWrite (1, start1, size1, start2, size2);
                    if (size1 > 0) sparkBuffer[start1] = { (float) (heldNotes[heldCount - 1] + 12), 1.0f, false };
                    sparkFifo.finishedWrite (size1);
                }
            }
        }

        // Filter-LFO synchron zum Host, alle 16 Samples
        if ((i & 15) == 0)
        {
            const double phase = pos / lfoBeats[juce::jlimit (0, 5, p.lfoRate)];
            lfoValue = std::sin ((float) (phase - std::floor (phase)) * twoPi);
            const float fc = juce::jlimit (30.0f, (float) sr * 0.45f, p.cutoff * std::pow (2.0f, lfoValue * p.lfoDepth * 2.0f));
            filt1.setCutoffFrequency (fc);
            filt2.setCutoffFrequency (juce::jmin (fc * 1.4f, (float) sr * 0.45f));
        }
        if ((i & 31) == 0 && heldCount > 0 && rng.nextFloat() < sparkChance)
            spawnSpark();

        // Stimmen
        float padL = 0.0f, padR = 0.0f;
        for (auto& v : voices)
        {
            if (! v.active) continue;

            if (v.gateLeft > 0 && --v.gateLeft == 0) v.stage = 2;
            if (v.stage == 0)
            {
                v.env += v.attackStep;
                if (v.env >= v.peak) { v.env = v.peak; v.stage = 1; }
            }
            else if (v.stage == 2)
            {
                v.env *= v.relCoef;
                if (v.env < 1.0e-5f) { v.active = false; continue; }
            }

            float sl = 0.0f, sr2 = 0.0f;
            for (int o = 0; o < v.nOsc; ++o)
            {
                float& t = v.phase[o];
                const float dt = v.inc[o];
                float x;
                switch (v.wave)
                {
                    case 0:  x = 2.0f * t - 1.0f - polyBlep (t, dt); break;
                    case 1:  { float t2 = t + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
                               x = (t < 0.5f ? 1.0f : -1.0f) + polyBlep (t, dt) - polyBlep (t2, dt); break; }
                    case 2:  x = 1.0f - 4.0f * std::abs (t - 0.5f); break;
                    default: x = std::sin (t * twoPi); break;
                }
                t += dt;
                if (t >= 1.0f) t -= 1.0f;
                sl += x * v.gainL[o];
                sr2 += x * v.gainR[o];
            }
            padL += sl * v.env;
            padR += sr2 * v.env;
        }

        // 24-dB-Tiefpass
        padL = filt2.processSample (0, filt1.processSample (0, padL));
        padR = filt2.processSample (1, filt1.processSample (1, padR));

        // Chorus
        chorusBuf[0][(size_t) chorusPos] = padL;
        chorusBuf[1][(size_t) chorusPos] = padR;
        float wet[2];
        for (int c = 0; c < 2; ++c)
        {
            chorusPhase[c] += chorusInc[c];
            if (chorusPhase[c] > twoPi) chorusPhase[c] -= twoPi;
            const float d = (0.012f + 0.005f * (float) c + 0.0028f * std::sin (chorusPhase[c])) * (float) sr;
            wet[c] = readLine (chorusBuf[c], chorusPos, d);
        }
        if (++chorusPos >= (int) chorusBuf[0].size()) chorusPos = 0;
        const float ch = chorusS.getNextValue() * 0.8f;
        padL += wet[0] * ch;
        padR += wet[1] * ch;

        // Sterne
        float spk = 0.0f, spkL = 0.0f, spkR = 0.0f;
        for (auto& s : sparks)
        {
            if (! s.active) continue;
            if (s.attacking)
            {
                s.amp += s.attackStep;
                if (s.amp >= s.peak) { s.amp = s.peak; s.attacking = false; }
            }
            else
            {
                s.amp *= s.decayCoef;
                if (s.amp < 0.0001f) { s.active = false; continue; }
            }
            s.index = juce::jmax (1.0f, s.index * s.indexCoef);
            s.mod += s.f * 3.51f / (float) sr;
            if (s.mod >= 1.0f) s.mod -= 1.0f;
            s.car += (s.f + std::sin (s.mod * twoPi) * s.index) / (float) sr;
            s.car -= std::floor (s.car);
            const float x = std::sin (s.car * twoPi) * s.amp;
            spkL += x * s.gL;
            spkR += x * s.gR;
        }
        spkL = sparkHP.processSample (0, spkL);
        spkR = sparkHP.processSample (1, spkR);
        spk = 0.5f * (spkL + spkR);

        // Ping-Pong-Echo
        const float dly = delayMixS.getNextValue();
        const float wobble = 0.0007f * (float) sr * std::sin (wobblePhase);
        wobblePhase += twoPi * 0.4f / (float) sr;
        if (wobblePhase > twoPi) wobblePhase -= twoPi;
        const float dTime = delaySamples.getNextValue() + wobble;
        const float dL = readLine (delayBuf[0], delayPos, dTime);
        const float dR = readLine (delayBuf[1], delayPos, dTime);
        const float fb = fbS.getNextValue();
        const float toR = std::tanh (fbHigh.processSample (0, fbLow.processSample (0, dL))) * fb;
        const float toL = std::tanh (fbHigh.processSample (1, fbLow.processSample (1, dR))) * fb;
        const float send = 0.5f * (padL + padR) * dly * 0.9f + spk * (0.35f + dly * 0.6f);
        delayBuf[0][(size_t) delayPos] = send + toL;
        delayBuf[1][(size_t) delayPos] = toR;
        if (++delayPos >= (int) delayBuf[0].size()) delayPos = 0;
        const float echoL = dL * 0.925f + dR * 0.075f;
        const float echoR = dL * 0.075f + dR * 0.925f;

        // Hall
        const float rv = revMixS.getNextValue() * 0.9f;
        float revL, revR;
        reverb.process (padL * rv + echoL * 0.5f + spkL * 0.9f, padR * rv + echoR * 0.5f + spkR * 0.9f, revL, revR);

        // Vinyl
        float nz[2];
        const float nGain = noiseS.getNextValue();
        for (int c = 0; c < 2; ++c)
        {
            const float w = rng.nextFloat() * 2.0f - 1.0f;
            noiseState[c] = 0.97f * noiseState[c] + 0.03f * w;
            float x = noiseState[c] * 0.9f + (rng.nextFloat() * 2.0f - 1.0f) * 0.05f;
            if (rng.nextFloat() < 0.00025f) x += (rng.nextFloat() < 0.5f ? -1.0f : 1.0f) * (0.3f + rng.nextFloat() * 0.6f);
            nz[c] = noiseBP.processSample (c, x) * nGain * nGain * 0.18f;
        }

        const float g = outS.getNextValue();
        const float oL = std::tanh ((padL + echoL + revL + spkL * 0.55f + nz[0]) * g);
        const float oR = std::tanh ((padR + echoR + revR + spkR * 0.55f + nz[1]) * g);
        left[i] = oL;
        right[i] = oR;

        peak = juce::jmax (peak, std::abs (oL), std::abs (oR));
        scope[sp] = 0.5f * (oL + oR);
        sp = (sp + 1) & (scopeSize - 1);
    }

    scopePos.store (sp);
    level.store (juce::jmax (peak, level.load() * 0.85f));
}
