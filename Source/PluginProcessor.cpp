#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Delay-Divisionen in Vierteln (Beats)
    const juce::StringArray delayDivNames { "1/16", "1/16 D", "1/8", "1/8 D", "1/4", "1/4 D", "1/2" };
    const float delayDivBeats[] { 0.25f, 0.375f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };

    const juce::StringArray pumpRateNames { "1/4", "1/8", "1/2" };
    const double pumpRateBeats[] { 1.0, 0.5, 2.0 };

    juce::NormalisableRange<float> freqRange (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    struct Preset
    {
        const char* name;
        std::initializer_list<std::pair<const char*, float>> values;
    };

    const Preset presets[] = {
        { "Init", {} },
        { "Deep Chord Dub", { { "lowCut", 30.0f }, { "monoFreq", 120.0f }, { "subGain", 3.0f },
                              { "drive", 4.0f }, { "driveMix", 0.4f }, { "filterType", 0.0f },
                              { "cutoff", 650.0f }, { "reso", 0.45f }, { "envAmt", 0.45f },
                              { "envRelease", 220.0f }, { "delayDiv", 3.0f }, { "feedback", 0.62f },
                              { "delayLowCut", 280.0f }, { "delayTone", 2200.0f }, { "delayMix", 0.35f },
                              { "pingPong", 1.0f }, { "pumpDepth", 0.25f } } },
        { "Rolling Sub", { { "lowCut", 28.0f }, { "monoFreq", 150.0f }, { "subFreq", 50.0f }, { "subGain", 6.0f },
                           { "drive", 8.0f }, { "driveTone", 1800.0f }, { "driveMix", 0.55f },
                           { "cutoff", 420.0f }, { "reso", 0.3f }, { "envAmt", 0.65f }, { "envAttack", 2.0f },
                           { "envRelease", 120.0f }, { "delayMix", 0.0f }, { "pumpDepth", 0.45f },
                           { "pumpRelease", 0.55f } } },
        { "Warehouse Pump", { { "lowCut", 35.0f }, { "subGain", 4.0f }, { "drive", 12.0f }, { "driveMix", 0.7f },
                              { "filterType", 0.0f }, { "cutoff", 900.0f }, { "reso", 0.55f }, { "envAmt", 0.3f },
                              { "delayDiv", 2.0f }, { "feedback", 0.4f }, { "delayLowCut", 400.0f },
                              { "delayMix", 0.2f }, { "pumpRate", 0.0f }, { "pumpDepth", 0.75f },
                              { "pumpRelease", 0.4f } } },
        { "Echo Chamber", { { "monoFreq", 100.0f }, { "cutoff", 1200.0f }, { "reso", 0.6f }, { "envAmt", -0.35f },
                            { "filterType", 1.0f }, { "delayDiv", 5.0f }, { "feedback", 0.78f },
                            { "delayLowCut", 350.0f }, { "delayTone", 1500.0f }, { "delayMix", 0.5f },
                            { "pingPong", 1.0f }, { "pumpDepth", 0.15f } } },
    };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DubBassProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dB  = AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; })
                   .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });
    auto hz  = AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz"
                                                                                         : String (roundToInt (v)) + " Hz"; })
                   .withValueFromStringFunction ([] (const String& t) { auto v = t.getFloatValue();
                                                                         return t.containsIgnoreCase ("k") ? v * 1000.0f : v; });
    auto ms  = AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; })
                   .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });
    auto pct = AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
                   .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue() / 100.0f; });

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "inGain", 1 }, "Input", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dB));

    // Low End
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lowCut", 1 }, "Low Cut", freqRange (10.0f, 150.0f, 35.0f), 20.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "monoFreq", 1 }, "Mono Below", freqRange (20.0f, 400.0f, 120.0f), 120.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "subFreq", 1 }, "Sub Freq", freqRange (30.0f, 120.0f, 55.0f), 55.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "subGain", 1 }, "Sub Boost", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dB));

    // Drive
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive", NormalisableRange<float> (0.0f, 30.0f, 0.1f), 0.0f, dB));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "driveTone", 1 }, "Drive Tone", freqRange (300.0f, 12000.0f, 2500.0f), 4000.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "driveMix", 1 }, "Drive Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));

    // Envelope Filter
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "filterType", 1 }, "Filter Type", StringArray { "Lowpass", "Bandpass", "Highpass" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "cutoff", 1 }, "Cutoff", freqRange (40.0f, 18000.0f, 800.0f), 18000.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "reso", 1 }, "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "envAmt", 1 }, "Env Amount", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "envAttack", 1 }, "Env Attack", freqRange (0.1f, 100.0f, 10.0f), 5.0f, ms));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "envRelease", 1 }, "Env Release", freqRange (10.0f, 1000.0f, 150.0f), 150.0f, ms));

    // Dub Delay
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "delayDiv", 1 }, "Delay Time", delayDivNames, 3));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "feedback", 1 }, "Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.5f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "delayLowCut", 1 }, "Delay Low Cut", freqRange (40.0f, 2000.0f, 250.0f), 250.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "delayTone", 1 }, "Delay Tone", freqRange (500.0f, 16000.0f, 3000.0f), 3000.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "delayMix", 1 }, "Delay Mix", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "pingPong", 1 }, "Ping Pong", StringArray { "Off", "On" }, 1));

    // Pump
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "pumpRate", 1 }, "Pump Rate", pumpRateNames, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "pumpDepth", 1 }, "Pump Depth", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "pumpRelease", 1 }, "Pump Release", NormalisableRange<float> (0.05f, 1.0f), 0.5f, pct));

    // Master
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Dry/Wet", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "outGain", 1 }, "Output", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dB));

    return { p.begin(), p.end() };
}

//==============================================================================
DubBassProcessor::DubBassProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DubBass", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pInGain = get ("inGain");       pLowCut = get ("lowCut");         pMonoFreq = get ("monoFreq");
    pSubFreq = get ("subFreq");     pSubGain = get ("subGain");       pDrive = get ("drive");
    pDriveTone = get ("driveTone"); pDriveMix = get ("driveMix");     pFilterType = get ("filterType");
    pCutoff = get ("cutoff");       pReso = get ("reso");             pEnvAmt = get ("envAmt");
    pEnvAttack = get ("envAttack"); pEnvRelease = get ("envRelease"); pDelayDiv = get ("delayDiv");
    pFeedback = get ("feedback");   pDelayLowCut = get ("delayLowCut"); pDelayTone = get ("delayTone");
    pDelayMix = get ("delayMix");   pPingPong = get ("pingPong");     pPumpRate = get ("pumpRate");
    pPumpDepth = get ("pumpDepth"); pPumpRelease = get ("pumpRelease"); pMix = get ("mix");
    pOutGain = get ("outGain");
}

bool DubBassProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void DubBassProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    lowCut.prepare (spec);      lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    xoverLow.prepare (spec);    xoverLow.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    xoverHigh.prepare (spec);   xoverHigh.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    envFilter.prepare (spec);
    delaySendHP.prepare (spec); delaySendHP.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    delayFbLP.prepare (spec);   delayFbLP.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    for (int ch = 0; ch < 2; ++ch)
    {
        subShelf[ch].reset();
        driveTone[ch].reset();
    }
    lastSubGain = -100.0f; lastSubFreq = -1.0f; lastTone = -1.0f;

    delayBuffer.setSize (2, (int) (sampleRate * 4.5) + 4);
    delayBuffer.clear();
    delayWritePos = 0;
    envelope = 0.0f;
    freeRunPpq = 0.0;

    for (auto* s : { &inGainS, &outGainS, &driveS, &driveMixS, &delayMixS, &feedbackS, &mixS, &pumpDepthS })
        s->reset (sampleRate, 0.03);
    delayTimeSmoothed.reset (sampleRate, 0.25);

    inGainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    outGainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    driveS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pDrive->load()));
    driveMixS.setCurrentAndTargetValue (pDriveMix->load());
    delayMixS.setCurrentAndTargetValue (pDelayMix->load());
    feedbackS.setCurrentAndTargetValue (pFeedback->load());
    mixS.setCurrentAndTargetValue (pMix->load());
    pumpDepthS.setCurrentAndTargetValue (pPumpDepth->load());
    delayTimeSmoothed.setCurrentAndTargetValue ((float) (sampleRate * 60.0 / 124.0 * 0.75));

    updateFilters();
}

void DubBassProcessor::updateFilters()
{
    lowCut.setCutoffFrequency (pLowCut->load());
    xoverLow.setCutoffFrequency (pMonoFreq->load());
    xoverHigh.setCutoffFrequency (pMonoFreq->load());
    delaySendHP.setCutoffFrequency (pDelayLowCut->load());
    delayFbLP.setCutoffFrequency (pDelayTone->load());

    const auto ft = (int) pFilterType->load();
    envFilter.setType (ft == 0 ? juce::dsp::StateVariableTPTFilterType::lowpass
                     : ft == 1 ? juce::dsp::StateVariableTPTFilterType::bandpass
                               : juce::dsp::StateVariableTPTFilterType::highpass);
    // Resonanz 0..1 -> Q 0.5..12
    envFilter.setResonance (0.5f + std::pow (pReso->load(), 2.0f) * 11.5f);

    const float subGain = pSubGain->load(), subFreq = pSubFreq->load();
    if (subGain != lastSubGain || subFreq != lastSubFreq)
    {
        auto c = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, subFreq, 0.8f, juce::Decibels::decibelsToGain (subGain));
        for (auto& f : subShelf) *f.coefficients = *c;
        lastSubGain = subGain; lastSubFreq = subFreq;
    }

    const float tone = pDriveTone->load();
    if (tone != lastTone)
    {
        auto c = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, juce::jmin (tone, (float) sr * 0.45f), 0.707f);
        for (auto& f : driveTone) *f.coefficients = *c;
        lastTone = tone;
    }
}

float DubBassProcessor::readDelay (int channel, float delaySamples) const
{
    const int size = delayBuffer.getNumSamples();
    float readPos = (float) delayWritePos - delaySamples;
    while (readPos < 0.0f) readPos += (float) size;

    const int i0 = (int) readPos;
    const int i1 = (i0 + 1) % size;
    const float frac = readPos - (float) i0;
    const float* d = delayBuffer.getReadPointer (channel);
    return d[i0] + frac * (d[i1] - d[i0]);
}

void DubBassProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    updateFilters();

    // --- Tempo / Position vom Host ---
    double bpm = 124.0, ppq = freeRunPpq;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (30.0, 300.0, *b);
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition()) { ppq = *q; playing = true; }
        }
    const double ppqPerSample = bpm / (60.0 * sr);

    // --- Ziele für Glättung ---
    inGainS.setTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    outGainS.setTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    driveS.setTargetValue (juce::Decibels::decibelsToGain (pDrive->load()));
    driveMixS.setTargetValue (pDriveMix->load());
    delayMixS.setTargetValue (pDelayMix->load());
    feedbackS.setTargetValue (pFeedback->load());
    mixS.setTargetValue (pMix->load());
    pumpDepthS.setTargetValue (pPumpDepth->load());

    const int divIdx = juce::jlimit (0, delayDivNames.size() - 1, (int) pDelayDiv->load());
    const float maxDelay = (float) delayBuffer.getNumSamples() - 4.0f;
    delayTimeSmoothed.setTargetValue (juce::jlimit (1.0f, maxDelay, (float) (delayDivBeats[divIdx] * 60.0 / bpm * sr)));

    const int rateIdx = juce::jlimit (0, 2, (int) pPumpRate->load());
    const double pumpBeats = pumpRateBeats[rateIdx];
    const float pumpRelease = pPumpRelease->load();

    const float cutoff = pCutoff->load();
    const float envAmt = pEnvAmt->load();
    const float attCoef = std::exp (-1.0f / (0.001f * pEnvAttack->load() * (float) sr));
    const float relCoef = std::exp (-1.0f / (0.001f * pEnvRelease->load() * (float) sr));
    const float maxCut = (float) sr * 0.45f;
    const bool pingPong = pPingPong->load() > 0.5f;
    const bool stereo = numIn > 1;

    float* L = buffer.getWritePointer (0);
    float* R = stereo ? buffer.getWritePointer (1) : nullptr;
    float* dL = delayBuffer.getWritePointer (0);
    float* dR = delayBuffer.getWritePointer (1);
    const int dSize = delayBuffer.getNumSamples();

    float peak = 0.0f, lastPump = 1.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        const float inG = inGainS.getNextValue();
        const float dryL = L[n];
        const float dryR = stereo ? R[n] : dryL;
        float x[2] = { dryL * inG, dryR * inG };

        // 1) Low Cut + Mono Bass + Sub Shelf
        for (int ch = 0; ch < 2; ++ch)
            x[ch] = lowCut.processSample (ch, x[ch]);

        const float lowL = xoverLow.processSample (0, x[0]);
        const float lowR = xoverLow.processSample (1, x[1]);
        const float hiL = xoverHigh.processSample (0, x[0]);
        const float hiR = xoverHigh.processSample (1, x[1]);
        const float lowMono = 0.5f * (lowL + lowR);
        x[0] = subShelf[0].processSample (lowMono + hiL);
        x[1] = subShelf[1].processSample (lowMono + hiR);

        // 2) Drive (asymmetrische Sättigung, parallel gemischt)
        const float dg = driveS.getNextValue();
        const float dMix = driveMixS.getNextValue();
        const float comp = 1.0f / std::sqrt (dg);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float bias = 0.15f;
            float s = std::tanh (x[ch] * dg + bias) - std::tanh (bias);
            s = driveTone[ch].processSample (s) * comp;
            x[ch] = x[ch] + dMix * (s - x[ch]);
        }

        // 3) Envelope Filter
        const float level = juce::jmax (std::abs (x[0]), std::abs (x[1]));
        const float coef = level > envelope ? attCoef : relCoef;
        envelope = level + coef * (envelope - level);

        if ((n & 7) == 0)
        {
            const float envNorm = juce::jlimit (0.0f, 1.0f, envelope * 2.0f);
            const float fc = juce::jlimit (20.0f, maxCut, cutoff * std::exp2 (envAmt * 5.0f * envNorm));
            envFilter.setCutoffFrequency (fc);
        }
        for (int ch = 0; ch < 2; ++ch)
            x[ch] = envFilter.processSample (ch, x[ch]);

        // 4) Dub Delay (nur Signal oberhalb Delay-Low-Cut geht ins Delay -> Bass bleibt sauber)
        const float dTime = delayTimeSmoothed.getNextValue();
        const float fb = feedbackS.getNextValue();
        const float dlMix = delayMixS.getNextValue();

        const float tapL = readDelay (0, dTime);
        const float tapR = readDelay (1, dTime);
        const float fbL = std::tanh (delayFbLP.processSample (0, tapL) * 1.2f) / 1.2f;
        const float fbR = std::tanh (delayFbLP.processSample (1, tapR) * 1.2f) / 1.2f;
        const float sendL = delaySendHP.processSample (0, x[0]);
        const float sendR = delaySendHP.processSample (1, x[1]);

        if (pingPong)
        {
            dL[delayWritePos] = 0.5f * (sendL + sendR) + fb * fbR;
            dR[delayWritePos] = fb * fbL;
        }
        else
        {
            dL[delayWritePos] = sendL + fb * fbL;
            dR[delayWritePos] = sendR + fb * fbR;
        }
        delayWritePos = (delayWritePos + 1) % dSize;

        x[0] += dlMix * tapL;
        x[1] += dlMix * tapR;

        // 5) Pump (tempo-synchrones Ducking)
        const double phase = std::fmod (ppq, pumpBeats) / pumpBeats;
        const float t = juce::jlimit (0.0f, 1.0f, (float) phase / pumpRelease);
        const float shape = t * t * (3.0f - 2.0f * t);
        const float pump = 1.0f - pumpDepthS.getNextValue() * (1.0f - shape);
        lastPump = pump;
        ppq += ppqPerSample;

        // 6) Mix + Output
        const float wetMix = mixS.getNextValue();
        const float outG = outGainS.getNextValue();
        const float oL = (dryL + wetMix * (x[0] * pump - dryL)) * outG;
        const float oR = (dryR + wetMix * (x[1] * pump - dryR)) * outG;

        L[n] = oL;
        if (stereo) R[n] = oR;
        peak = juce::jmax (peak, std::abs (oL), std::abs (oR));
    }

    if (! playing)
        freeRunPpq = std::fmod (ppq, 64.0);

    envDisplay.store (envelope);
    pumpDisplay.store (lastPump);
    outLevel.store (juce::jmax (peak, outLevel.load() * 0.85f));
}

//==============================================================================
int DubBassProcessor::getNumPrograms() { return (int) std::size (presets); }

const juce::String DubBassProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? presets[index].name : "";
}

void DubBassProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;

    // Erst alles auf Default, dann Preset-Werte setzen
    for (auto* param : getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            p->setValueNotifyingHost (p->getDefaultValue());

    for (auto& [id, value] : presets[index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void DubBassProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DubBassProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* DubBassProcessor::createEditor() { return new DubBassEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DubBassProcessor(); }
