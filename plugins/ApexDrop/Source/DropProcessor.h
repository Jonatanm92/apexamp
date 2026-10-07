#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "apex/dsp/PitchShifter.h"
#include "apex/ui/PresetManager.h"

/**
 * Apex Drop
 * ---------
 * Thin JUCE wrapper around apex::dsp::PitchShifter plus a sub-octave layer.
 * All DSP lives in libs/apex-dsp; this class owns parameters, presets, state
 * and latency reporting.
 */
class DropProcessor : public juce::AudioProcessor,
                      private juce::Timer
{
public:
    DropProcessor();
    ~DropProcessor() override;

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

    /** Latency of a mode at the current sample rate, for the display. */
    double getLatencyMs (apex::dsp::PitchMode mode) const;

    juce::UndoManager undoManager { 30000, 40 };
    juce::AudioProcessorValueTreeState apvts;
    apex::ui::PresetManager presets;

    std::atomic<float> inputMagnitude  { 0.0f };
    std::atomic<float> outputMagnitude { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static std::vector<apex::ui::FactoryPreset> factoryPresets();
    void timerCallback() override;
    apex::dsp::PitchMode currentModeParam() const;

    apex::dsp::PitchShifter shifter, subShifter;
    juce::AudioBuffer<float> subBuffer;
    juce::SmoothedValue<float> outputGain, subLevel;
    double currentRate = 48000.0;
    struct OnePole2 { float a = 0.0f, z1 = 0.0f, z2 = 0.0f; float process (float x) { z1 += a * (x - z1); z2 += a * (z1 - z2); return z2; } };
    OnePole2 subFilter[2];

    // Latency changes with the mode; the host is told from the message thread.
    std::atomic<int> pendingLatency { -1 };

    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* shiftParam = nullptr;
    std::atomic<float>* fineParam  = nullptr;
    std::atomic<float>* modeParam  = nullptr;
    std::atomic<float>* bodyParam  = nullptr;
    std::atomic<float>* subParam   = nullptr;
    std::atomic<float>* mixParam   = nullptr;
    std::atomic<float>* outParam   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropProcessor)
};
