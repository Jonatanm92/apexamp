#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "NamEngine.h"
#include "apex/ui/PresetManager.h"
#include "apex/ui/TunerFeed.h"

class ApexAmpProcessor : public juce::AudioProcessor,
                         private juce::Timer
{
public:
    ApexAmpProcessor();
    ~ApexAmpProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    const juce::String getName() const override { return "ApexAmp"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::UndoManager undoManager { 60000, 60 };
    juce::AudioProcessorValueTreeState apvts;
    apex::ui::PresetManager presets;
    apex::ui::TunerFeed tunerFeed;

    // Live metering for the editor (peak magnitude per block, linear 0..1+).
    std::atomic<float> inputMagnitude  { 0.0f };
    std::atomic<float> outputMagnitude { 0.0f };

    // User-loaded cab IR / NAM rig (called from the editor / message thread).
    bool loadUserIr  (const juce::File& f) { return engine.loadUserIr (f); }
    bool loadUserRig (const juce::File& f) { return engine.loadUserRig (f); }
    bool hasUserIr()  const noexcept { return engine.hasUserIr(); }
    bool hasUserRig() const noexcept { return engine.hasUserRig(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    NamEngine::Params gatherParams();
    int currentLatency() const;
    void timerCallback() override;

    NamEngine engine;
    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* dropOnParam = nullptr;
    std::atomic<int> pendingLatency { -1 };
    int reportedLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApexAmpProcessor)
};
