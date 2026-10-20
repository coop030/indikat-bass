#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PlutoEngine.h"

class PlutoProcessor : public juce::AudioProcessor
{
public:
    PlutoProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Neue zufällige Szene (vom Planeten aufgerufen)
    void randomize();
    juce::uint32 getSeed() const { return seed.load(); }

    PlutoEngine engine;
    std::atomic<float> bpmShown { 122.0f };

private:
    void setSeed (juce::uint32);

    std::atomic<juce::uint32> seed { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlutoProcessor)
};
