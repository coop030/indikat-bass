#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "StarPadEngine.h"

// INDIKAT Star Pad: Pad-Synth (Instrument). MIDI-Noten und die 8 Akkord-Pads der Oberfläche
// spielen die Engine; Tempo und Raster kommen aus dem Host.
class StarPadProcessor : public juce::AudioProcessor
{
public:
    StarPadProcessor();
    ~StarPadProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    const juce::String getProgramDescription (int index) const;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    StarPadParams readParams() const;

    juce::AudioProcessorValueTreeState apvts;
    StarPadEngine engine;
    juce::MidiKeyboardState keyboardState;

    // Akkord-Pad der Oberfläche (-1 = keins), wird im Audio-Thread übernommen
    std::atomic<int> padRequest { -1 };
    std::atomic<float> hostBpm { 122.0f };

private:
    double sr = 44100.0;
    int currentProgram = 0;
    double freeRunPpq = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StarPadProcessor)
};
