#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Delay-Divisionen in Vierteln (Beats)
    const juce::StringArray delayDivNames { "1/16", "1/16 D", "1/8", "1/8 D", "1/4", "1/4 D", "1/2" };
    const float delayDivBeats[] { 0.25f, 0.375f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };

    // Filter-LFO: Perioden in Beats
    const juce::StringArray lfoRateNames { "1/4", "1/2", "1 BAR", "2 BARS", "4 BARS" };
    const double lfoRateBeats[] { 1.0, 2.0, 4.0, 8.0, 16.0 };

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

    const Preset fxPresets[] = {
        { "Init", {} },
        { "Deep Chord Dub", { { "lowCut", 30.0f }, { "monoFreq", 120.0f }, { "subGain", 3.0f },
                              { "drive", 4.0f }, { "driveMix", 0.4f }, { "filterType", 0.0f },
                              { "cutoff", 650.0f }, { "reso", 0.45f }, { "envAmt", 0.45f },
                              { "envRelease", 220.0f }, { "lfoRate", 3 }, { "lfoDepth", 0.25f },
                              { "delayDiv", 3.0f }, { "feedback", 0.62f },
                              { "delayLowCut", 280.0f }, { "delayTone", 2200.0f }, { "delayMix", 0.35f },
                              { "pingPong", 1.0f }, { "revSize", 0.7f }, { "revTone", 0.35f }, { "revMix", 0.3f } } },
        { "Rolling Sub", { { "lowCut", 28.0f }, { "monoFreq", 150.0f }, { "subFreq", 50.0f }, { "subGain", 6.0f },
                           { "drive", 8.0f }, { "driveTone", 1800.0f }, { "driveMix", 0.55f },
                           { "cutoff", 420.0f }, { "reso", 0.3f }, { "envAmt", 0.65f }, { "envAttack", 2.0f },
                           { "envRelease", 120.0f }, { "delayMix", 0.0f } } },
        { "Warehouse Room", { { "lowCut", 35.0f }, { "subGain", 4.0f }, { "drive", 12.0f }, { "driveMix", 0.7f },
                              { "filterType", 0.0f }, { "cutoff", 900.0f }, { "reso", 0.55f }, { "envAmt", 0.3f },
                              { "delayDiv", 2.0f }, { "feedback", 0.4f }, { "delayLowCut", 400.0f },
                              { "delayMix", 0.2f }, { "revSize", 0.35f }, { "revTone", 0.5f }, { "revMix", 0.18f } } },
        { "Echo Chamber", { { "monoFreq", 100.0f }, { "cutoff", 1200.0f }, { "reso", 0.6f }, { "envAmt", -0.35f },
                            { "filterType", 1.0f }, { "lfoRate", 4 }, { "lfoDepth", 0.3f }, { "delayDiv", 5.0f },
                            { "feedback", 0.78f }, { "delayLowCut", 350.0f }, { "delayTone", 1500.0f }, { "delayMix", 0.5f },
                            { "pingPong", 1.0f }, { "revSize", 0.85f }, { "revTone", 0.3f }, { "revMix", 0.35f } } },
    };

    // Synth-Presets: nur Abweichungen von den Defaults (siehe createLayout)
    const Preset synthPresets[] = {
        { "Init Saw", { { "outGain", 2.4f } } },
        { "Deep Sub Pluck", { { "osc1Wave", 0 }, { "osc2Wave", 1 }, { "osc2Oct", 1 }, { "osc2Level", 0.3f }, { "subLevel", 0.3f },
                              { "ampDecay", 250 }, { "ampSustain", 0.0f }, { "ampRelease", 120 },
                              { "cutoff", 300 }, { "reso", 0.25f }, { "envAmt", 0.55f }, { "envRelease", 180 },
                              { "drive", 6 }, { "driveMix", 0.4f }, { "outGain", 3.2f } } },
        { "Dub Chord Stab", { { "osc1Wave", 2 }, { "osc2Wave", 2 }, { "osc2Fine", 7 }, { "osc2Level", 0.6f },
                              { "chordOn", 1 }, { "chordType", 0 },
                              { "ampDecay", 400 }, { "ampSustain", 0.0f }, { "ampRelease", 250 },
                              { "cutoff", 350 }, { "reso", 0.5f }, { "envAmt", 0.6f }, { "envRelease", 260 },
                              { "lfoRate", 3 }, { "lfoDepth", 0.3f },
                              { "delayDiv", 3 }, { "feedback", 0.62f }, { "delayLowCut", 300 }, { "delayTone", 2200 },
                              { "delayMix", 0.35f }, { "revSize", 0.75f }, { "revTone", 0.35f }, { "revMix", 0.3f },
                              { "drift", 0.25f }, { "outGain", -1.4f } } },
        { "Minor Ninth Stab", { { "osc1Wave", 2 }, { "osc2Wave", 1 }, { "osc2Oct", 1 }, { "osc2Fine", -5 }, { "osc2Level", 0.3f },
                                { "chordOn", 1 }, { "chordType", 1 },
                                { "subLevel", 0.15f }, { "ampDecay", 320 }, { "ampSustain", 0.0f }, { "ampRelease", 300 },
                                { "cutoff", 420 }, { "reso", 0.45f }, { "envAmt", 0.5f }, { "envRelease", 220 },
                                { "delayDiv", 3 }, { "feedback", 0.7f }, { "delayLowCut", 350 }, { "delayTone", 1800 },
                                { "delayMix", 0.4f }, { "revSize", 0.8f }, { "revTone", 0.3f }, { "revMix", 0.35f },
                                { "drift", 0.3f }, { "outGain", 0.3f } } },
        { "Deep Space Chord", { { "osc1Wave", 2 }, { "osc2Wave", 1 }, { "osc2Oct", 1 }, { "osc2Level", 0.4f },
                                { "chordOn", 1 }, { "chordType", 2 },
                                { "ampDecay", 600 }, { "ampSustain", 0.0f }, { "ampRelease", 500 },
                                { "cutoff", 380 }, { "reso", 0.4f }, { "envAmt", 0.45f }, { "envRelease", 300 },
                                { "lfoRate", 4 }, { "lfoDepth", 0.4f },
                                { "delayDiv", 3 }, { "feedback", 0.65f }, { "delayMix", 0.3f },
                                { "revSize", 0.95f }, { "revTone", 0.25f }, { "revMix", 0.45f }, { "drift", 0.3f }, { "outGain", -0.3f } } },
        { "Rubber Bass", { { "osc1Wave", 3 }, { "osc2Wave", 2 }, { "osc2Oct", -1 }, { "osc2Level", 0.5f },
                           { "ampDecay", 300 }, { "ampSustain", 0.5f }, { "ampRelease", 90 },
                           { "cutoff", 250 }, { "reso", 0.6f }, { "envAmt", 0.7f }, { "envRelease", 140 },
                           { "glide", 60 }, { "legato", 1 }, { "drive", 8 }, { "driveMix", 0.5f }, { "outGain", -2.6f } } },
        { "Warm Round", { { "osc1Wave", 1 }, { "osc2Wave", 0 }, { "osc2Oct", -1 }, { "osc2Level", 0.6f },
                          { "ampSustain", 0.8f }, { "ampRelease", 150 }, { "cutoff", 1200 }, { "envAmt", 0.15f },
                          { "drive", 3 }, { "driveMix", 0.5f }, { "outGain", -1.5f } } },
        { "Minimal Click Bass", { { "osc1Wave", 0 }, { "osc2Wave", 3 }, { "osc2Oct", 1 }, { "osc2Level", 0.15f },
                                  { "ampAttack", 0.5f }, { "ampDecay", 90 }, { "ampSustain", 0.0f }, { "ampRelease", 60 },
                                  { "cutoff", 2000 }, { "envAmt", 0.3f }, { "envRelease", 40 },
                                  { "drive", 10 }, { "driveTone", 3000 }, { "driveMix", 0.3f }, { "outGain", 6.1f } } },
        { "Acid Glide", { { "osc1Wave", 2 }, { "ampSustain", 0.8f }, { "ampRelease", 60 },
                          { "cutoff", 300 }, { "reso", 0.75f }, { "envAmt", 0.7f }, { "envRelease", 200 },
                          { "glide", 80 }, { "legato", 1 }, { "velocity", 0.8f }, { "drive", 12 }, { "driveMix", 0.6f },
                          { "outGain", -2.8f } } },
        { "Detuned Wobble", { { "osc1Wave", 2 }, { "osc2Wave", 2 }, { "osc2Fine", 12 }, { "osc2Level", 0.8f },
                              { "cutoff", 500 }, { "reso", 0.35f }, { "envAmt", 0.3f }, { "envAttack", 20 }, { "envRelease", 400 },
                              { "lfoRate", 1 }, { "lfoDepth", 0.25f },
                              { "drift", 0.4f }, { "delayDiv", 5 }, { "delayMix", 0.2f }, { "outGain", -1.5f } } },
        { "Sub Only", { { "osc1Wave", 0 }, { "subLevel", 0.5f }, { "ampSustain", 1.0f }, { "ampRelease", 80 },
                        { "cutoff", 18000 }, { "envAmt", 0.0f }, { "drive", 2 }, { "driveTone", 1500 }, { "driveMix", 0.2f },
                        { "monoFreq", 200 }, { "drift", 0.0f }, { "outGain", -2.8f } } },
        { "Warehouse Bass", { { "osc1Wave", 2 }, { "osc2Wave", 3 }, { "osc2Oct", -1 }, { "osc2Level", 0.6f },
                              { "cutoff", 600 }, { "reso", 0.4f }, { "envAmt", 0.45f }, { "envRelease", 200 },
                              { "revSize", 0.3f }, { "revMix", 0.12f }, { "drive", 10 }, { "driveMix", 0.6f },
                              { "outGain", -3.2f } } },
        { "Echo Dub Bass", { { "osc1Wave", 1 }, { "osc2Wave", 2 }, { "osc2Oct", 1 }, { "osc2Level", 0.35f },
                             { "ampDecay", 350 }, { "ampSustain", 0.3f }, { "cutoff", 700 }, { "envAmt", 0.4f },
                             { "delayDiv", 3 }, { "feedback", 0.7f }, { "delayLowCut", 400 }, { "delayTone", 1800 },
                             { "delayMix", 0.45f }, { "revSize", 0.65f }, { "revMix", 0.25f }, { "outGain", 4.1f } } },
        { "Hollow Pulse", { { "osc1Wave", 4 }, { "osc2Wave", 0 }, { "osc2Oct", -1 }, { "osc2Level", 0.5f },
                            { "cutoff", 900 }, { "reso", 0.3f }, { "envAmt", 0.3f }, { "drift", 0.2f }, { "outGain", -1.1f } } },
        { "Tape Deep", { { "osc1Wave", 2 }, { "cutoff", 220 }, { "reso", 0.2f }, { "envAmt", 0.35f }, { "envRelease", 300 },
                         { "lfoRate", 3 }, { "lfoDepth", 0.2f },
                         { "drive", 14 }, { "driveTone", 900 }, { "driveMix", 0.7f }, { "delayDiv", 4 }, { "feedback", 0.45f },
                         { "delayTone", 1200 }, { "delayMix", 0.2f }, { "drift", 0.5f }, { "outGain", -0.8f } } },
        { "Fifth Pluck", { { "osc1Wave", 0 }, { "osc2Wave", 1 }, { "osc2Semi", 7 }, { "osc2Level", 0.5f },
                           { "ampDecay", 180 }, { "ampSustain", 0.0f }, { "ampRelease", 100 },
                           { "cutoff", 1000 }, { "envAmt", 0.3f }, { "outGain", 3.4f } } },
        { "Night Drive", { { "osc1Wave", 2 }, { "osc2Wave", 4 }, { "osc2Oct", -1 }, { "osc2Fine", -7 }, { "osc2Level", 0.5f },
                           { "subLevel", 0.25f }, { "ampDecay", 500 }, { "ampSustain", 0.4f }, { "ampRelease", 200 },
                           { "filterType", 1 }, { "cutoff", 450 }, { "reso", 0.55f }, { "envAmt", 0.5f }, { "envRelease", 250 },
                           { "lfoRate", 2 }, { "lfoDepth", 0.35f },
                           { "delayDiv", 3 }, { "feedback", 0.55f }, { "delayMix", 0.25f },
                           { "revSize", 0.55f }, { "revMix", 0.2f }, { "outGain", 2.0f } } },
    };

    const Preset* const presets = DubBassProcessor::isSynth ? synthPresets : fxPresets;
    const int numPresets = DubBassProcessor::isSynth ? (int) std::size (synthPresets) : (int) std::size (fxPresets);
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

    if (isSynth)
        BassSynth::addParameters (p);

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "inGain", 1 }, isSynth ? "Synth Level" : "Input", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dB));

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
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "cutoff", 1 }, "Cutoff", freqRange (40.0f, 18000.0f, 800.0f), isSynth ? 500.0f : 18000.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "reso", 1 }, "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "envAmt", 1 }, "Env Amount", NormalisableRange<float> (-1.0f, 1.0f), isSynth ? 0.5f : 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "envAttack", 1 }, "Env Attack", freqRange (0.1f, 100.0f, 10.0f), isSynth ? 1.0f : 5.0f, ms));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "lfoRate", 1 }, "LFO Rate", lfoRateNames, 3));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lfoDepth", 1 }, "LFO Depth", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "envRelease", 1 }, isSynth ? "Env Decay" : "Env Release", freqRange (10.0f, 1000.0f, 150.0f), isSynth ? 200.0f : 150.0f, ms));

    // Dub Delay
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "delayDiv", 1 }, "Delay Time", delayDivNames, 3));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "feedback", 1 }, "Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.5f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "delayLowCut", 1 }, "Delay Low Cut", freqRange (40.0f, 2000.0f, 250.0f), 250.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "delayTone", 1 }, "Delay Tone", freqRange (500.0f, 16000.0f, 3000.0f), 3000.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "delayMix", 1 }, "Delay Mix", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "pingPong", 1 }, "Ping Pong", StringArray { "Off", "On" }, 1));

    // Reverb
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "revSize", 1 }, "Reverb Size", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "revTone", 1 }, "Reverb Tone", NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "revMix", 1 }, "Reverb Mix", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "revLowCut", 1 }, "Reverb Low Cut", StringArray { "Off", "On" }, 1));

    // Master
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Dry/Wet", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "outGain", 1 }, "Output", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dB));

    return { p.begin(), p.end() };
}

//==============================================================================
DubBassProcessor::DubBassProcessor()
    : AudioProcessor (isSynth ? BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                              : BusesProperties()
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
    pDelayMix = get ("delayMix");   pPingPong = get ("pingPong");     pMix = get ("mix");
    pRevSize = get ("revSize");     pRevTone = get ("revTone");       pRevMix = get ("revMix");
    pRevLowCut = get ("revLowCut"); pLfoRate = get ("lfoRate");       pLfoDepth = get ("lfoDepth");
    pOutGain = get ("outGain");

    if (isSynth)
        synth.init (apvts);
}

bool DubBassProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return isSynth ? layouts.getMainInputChannelSet().isDisabled() : layouts.getMainInputChannelSet() == out;
}

void DubBassProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    if (isSynth)
    {
        synth.prepare (sampleRate);
        synthMod.assign ((size_t) samplesPerBlock, 0.0f);
    }
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

    for (auto* s : { &inGainS, &outGainS, &driveS, &driveMixS, &delayMixS, &feedbackS, &mixS, &revMixS, &lfoDepthS })
        s->reset (sampleRate, 0.03);
    delayTimeSmoothed.reset (sampleRate, 0.25);

    inGainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    outGainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    driveS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pDrive->load()));
    driveMixS.setCurrentAndTargetValue (pDriveMix->load());
    delayMixS.setCurrentAndTargetValue (pDelayMix->load());
    feedbackS.setCurrentAndTargetValue (pFeedback->load());
    mixS.setCurrentAndTargetValue (pMix->load());
    revMixS.setCurrentAndTargetValue (pRevMix->load());
    lfoDepthS.setCurrentAndTargetValue (pLfoDepth->load());
    reverb.prepare (sampleRate);
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

void DubBassProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    int numIn = getTotalNumInputChannels();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (isSynth)
    {
        if (synthMod.size() < (size_t) numSamples)
            synthMod.resize ((size_t) numSamples); // Host hält sich nicht an die Blockgröße
        keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
        synth.render (midi, buffer.getWritePointer (0), synthMod.data(), numSamples);
        for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
        numIn = buffer.getNumChannels();
        midi.clear();
    }

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
    revMixS.setTargetValue (pRevMix->load());
    lfoDepthS.setTargetValue (pLfoDepth->load());
    reverb.setParameters (pRevSize->load(), pRevTone->load(), pRevLowCut->load() > 0.5f);

    const int divIdx = juce::jlimit (0, delayDivNames.size() - 1, (int) pDelayDiv->load());
    const float maxDelay = (float) delayBuffer.getNumSamples() - 4.0f;
    delayTimeSmoothed.setTargetValue (juce::jlimit (1.0f, maxDelay, (float) (delayDivBeats[divIdx] * 60.0 / bpm * sr)));

    const int lfoIdx = juce::jlimit (0, lfoRateNames.size() - 1, (int) pLfoRate->load());
    const double lfoBeats = lfoRateBeats[lfoIdx];

    const float cutoff = pCutoff->load();
    const float envAmt = pEnvAmt->load();
    const float attCoef = std::exp (-1.0f / (0.001f * pEnvAttack->load() * (float) sr));
    const float relCoef = std::exp (-1.0f / (0.001f * pEnvRelease->load() * (float) sr));
    const float maxCut = (float) sr * 0.45f;
    const bool pingPong = pPingPong->load() > 0.5f;
    const bool stereo = numIn > 1;
    const float* mod = isSynth ? synthMod.data() : nullptr;

    float* L = buffer.getWritePointer (0);
    float* R = stereo ? buffer.getWritePointer (1) : nullptr;
    float* dL = delayBuffer.getWritePointer (0);
    float* dR = delayBuffer.getWritePointer (1);
    const int dSize = delayBuffer.getNumSamples();

    float peak = 0.0f, lfoValue = 0.0f;
    int scopePos = scopeWritePos.load();

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

        // 3) Envelope Filter (FX: Envelope Follower, Synth: Filter-Hüllkurve)
        if (mod != nullptr)
            envelope = mod[n] * 0.5f;
        else
        {
            const float level = juce::jmax (std::abs (x[0]), std::abs (x[1]));
            const float coef = level > envelope ? attCoef : relCoef;
            envelope = level + coef * (envelope - level);
        }

        // Filter-LFO: Sinus synchron zur DAW-Position, bis ±2.5 Oktaven
        const float lfoDepth = lfoDepthS.getNextValue();
        if ((n & 7) == 0)
        {
            lfoValue = (float) std::sin (juce::MathConstants<double>::twoPi * std::fmod (ppq, lfoBeats) / lfoBeats);
            const float envNorm = juce::jlimit (0.0f, 1.0f, envelope * 2.0f);
            const float fc = juce::jlimit (20.0f, maxCut, cutoff * std::exp2 (envAmt * 5.0f * envNorm + lfoDepth * 2.5f * lfoValue));
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

        // 5) Reverb (Send nach dem Delay)
        float revL, revR;
        reverb.process (x[0], x[1], revL, revR);
        const float rvMix = revMixS.getNextValue();
        x[0] += rvMix * revL;
        x[1] += rvMix * revR;
        ppq += ppqPerSample;

        // 6) Mix + Output
        const float wetMix = mixS.getNextValue();
        const float outG = outGainS.getNextValue();
        const float oL = (dryL + wetMix * (x[0] - dryL)) * outG;
        const float oR = (dryR + wetMix * (x[1] - dryR)) * outG;

        L[n] = oL;
        if (stereo) R[n] = oR;
        peak = juce::jmax (peak, std::abs (oL), std::abs (oR));
        scopeBuffer[scopePos] = 0.5f * (oL + oR);
        scopePos = (scopePos + 1) & (scopeSize - 1);
    }
    scopeWritePos.store (scopePos);

    if (! playing)
        freeRunPpq = std::fmod (ppq, 64.0);

    envDisplay.store (envelope);
    lfoDisplay.store (lfoValue * pLfoDepth->load());
    reverbDisplay.store (juce::jlimit (0.0f, 1.0f, std::sqrt (reverb.getEnergy()) * 4.0f * pRevMix->load()));
    outLevel.store (juce::jmax (peak, outLevel.load() * 0.85f));
}

//==============================================================================
int DubBassProcessor::getNumPrograms() { return numPresets; }

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
