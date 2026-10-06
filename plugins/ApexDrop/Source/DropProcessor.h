#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "apex/dsp/PitchShifter.h"

/**
 * Apex Drop
 * ---------
 * Thin JUCE wrapper around apex::dsp::PitchShifter. All DSP lives in
 * libs/apex-dsp; this class only owns parameters, state and latency reporting.
 *
 * The editor is JUCE's generic one until the shared Apex design system
 * (libs/apex-ui) is built from the approved mockups.
 */
class DropProcessor : public juce::AudioProcessor,
                      private juce::Timer
{
public:
    DropProcessor();
    ~DropProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    const juce::String getName() const override { return "Apex Drop"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    apex::dsp::PitchMode currentModeParam() const;

    apex::dsp::PitchShifter shifter;
    juce::SmoothedValue<float> outputGain;

    // Latency changes with the mode; the host is told from the message thread.
    std::atomic<int> pendingLatency { -1 };

    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* shiftParam = nullptr;
    std::atomic<float>* fineParam  = nullptr;
    std::atomic<float>* modeParam  = nullptr;
    std::atomic<float>* bodyParam  = nullptr;
    std::atomic<float>* mixParam   = nullptr;
    std::atomic<float>* outParam   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropProcessor)
};
