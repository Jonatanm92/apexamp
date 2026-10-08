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
    double getTailLengthSeconds() const override;

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

    /** Echo time in ms (follows the host tempo when synced). */
    double getEchoMs() const;
    double getHostBpm() const noexcept { return hostBpm.load(); }

    /** Auto Input: listens for ~3 s of playing and sets the input trim so the
        guitar hits the amp at the level the rigs were captured for. */
    void startAutoInput();
    bool isAutoInputListening() const noexcept { return autoLearning.load(); }
    /** 0 = nothing yet, 1 = trim set, 2 = heard no guitar. */
    int getAutoInputOutcome() const noexcept { return autoOutcome.load(); }
    float getAutoInputTrim() const noexcept { return autoTrimResult.load(); }
    /** Peak level the Auto Input aims for (dBFS), from the target zone. */
    float getInputTargetDb() const;

    // Editor feeds: chug punch envelope (0..1), peak level into the amp (linear),
    // last output block for the scope.
    std::atomic<float> chugPunch { 0.0f }, ampDrive { 0.0f };
    double getLatencyMs() const { return 1000.0 * getLatencySamples() / juce::jmax (1.0, getSampleRate()); }

    /** Output waveform for the editor's scope: every 4th sample of the mono
        output in a ring (the editor reads the most recent part). */
    static constexpr int scopeSize = 2048;
    std::array<std::atomic<float>, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    /** Fraction of the block time spent processing (smoothed). */
    std::atomic<float> dspLoad { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    NamEngine::Params gatherParams();
    int currentLatency() const;
    void timerCallback() override;
    void listenForAutoInput (const juce::AudioBuffer<float>&, int channels, int numSamples);

    NamEngine engine;
    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* dropOnParam = nullptr;
    std::atomic<int> pendingLatency { -1 };
    int reportedLatency = -1;
    std::atomic<double> hostBpm { 120.0 };

    // Auto Input (histogram of 10 ms input peaks in 0.5 dB bins, -60..+6 dBFS)
    std::atomic<bool> autoLearning { false };
    std::atomic<int> autoOutcome { 0 };
    std::atomic<float> autoTrimResult { 0.0f };
    bool autoRunning = false, autoTrimApplied = false;
    std::array<int, 133> autoHistogram {};
    int autoPlayingSamples = 0, autoTotalSamples = 0, autoWindowCount = 0;
    float autoWindowPeak = 0.0f;
    int scopePhase = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApexAmpProcessor)
};
