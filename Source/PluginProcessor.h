#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

// Signalfluss:
// Input -> Low Cut -> Mono Bass (unterhalb Crossover) -> Sub Shelf -> Drive
//       -> Envelope Filter -> Dub Delay (Send, mit Low Cut) -> Pump -> Mix -> Output
class DubBassProcessor : public juce::AudioProcessor
{
public:
    DubBassProcessor();
    ~DubBassProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Für die GUI (Meter / Envelope-Anzeige)
    std::atomic<float> envDisplay { 0.0f };
    std::atomic<float> pumpDisplay { 1.0f };
    std::atomic<float> outLevel { 0.0f };

private:
    void updateFilters();
    float readDelay (int channel, float delaySamples) const;

    double sr = 44100.0;
    int currentProgram = 0;

    // Filter
    juce::dsp::StateVariableTPTFilter<float> lowCut;
    juce::dsp::LinkwitzRileyFilter<float> xoverLow, xoverHigh;
    juce::dsp::IIR::Filter<float> subShelf[2];
    juce::dsp::IIR::Filter<float> driveTone[2];
    juce::dsp::StateVariableTPTFilter<float> envFilter;
    juce::dsp::StateVariableTPTFilter<float> delaySendHP;
    juce::dsp::StateVariableTPTFilter<float> delayFbLP;

    float lastSubGain = -100.0f, lastSubFreq = -1.0f, lastTone = -1.0f;

    // Envelope Follower
    float envelope = 0.0f;

    // Delay
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;
    juce::SmoothedValue<float> delayTimeSmoothed;

    // Pump
    double freeRunPpq = 0.0;

    // Glättung
    juce::SmoothedValue<float> inGainS, outGainS, driveS, driveMixS, delayMixS, feedbackS, mixS, pumpDepthS;

    // Parameter-Pointer
    std::atomic<float>* pInGain;
    std::atomic<float>* pLowCut;
    std::atomic<float>* pMonoFreq;
    std::atomic<float>* pSubFreq;
    std::atomic<float>* pSubGain;
    std::atomic<float>* pDrive;
    std::atomic<float>* pDriveTone;
    std::atomic<float>* pDriveMix;
    std::atomic<float>* pFilterType;
    std::atomic<float>* pCutoff;
    std::atomic<float>* pReso;
    std::atomic<float>* pEnvAmt;
    std::atomic<float>* pEnvAttack;
    std::atomic<float>* pEnvRelease;
    std::atomic<float>* pDelayDiv;
    std::atomic<float>* pFeedback;
    std::atomic<float>* pDelayLowCut;
    std::atomic<float>* pDelayTone;
    std::atomic<float>* pDelayMix;
    std::atomic<float>* pPingPong;
    std::atomic<float>* pPumpRate;
    std::atomic<float>* pPumpDepth;
    std::atomic<float>* pPumpRelease;
    std::atomic<float>* pMix;
    std::atomic<float>* pOutGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DubBassProcessor)
};
