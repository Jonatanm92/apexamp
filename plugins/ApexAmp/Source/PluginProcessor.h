#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "NamEngine.h"

class ApexAmpProcessor : public juce::AudioProcessor
{
public:
    ApexAmpProcessor();
    ~ApexAmpProcessor() override = default;

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

    juce::AudioProcessorValueTreeState apvts;

    // Live metering for the editor (peak magnitude per block, linear 0..1+).
    std::atomic<float> inputMagnitude  { 0.0f };
    std::atomic<float> outputMagnitude { 0.0f };

    // Spectrum scope: single-producer (audio) / single-consumer (editor) FIFO.
    static constexpr int scopeFftOrder = 11;             // 2048-point FFT
    static constexpr int scopeFftSize  = 1 << scopeFftOrder;
    std::array<float, scopeFftSize>     scopeFifo {};
    std::array<float, scopeFftSize * 2> scopeFftData {};
    int scopeFifoIndex = 0;
    std::atomic<bool> scopeReady { false };

    inline void pushScopeSample (float s) noexcept
    {
        if (scopeFifoIndex == scopeFftSize)
        {
            if (! scopeReady.load (std::memory_order_acquire))
            {
                std::copy (scopeFifo.begin(), scopeFifo.end(), scopeFftData.begin());
                scopeReady.store (true, std::memory_order_release);
            }
            scopeFifoIndex = 0;
        }
        scopeFifo[(size_t) scopeFifoIndex++] = s;
    }

    // User-loaded cab IR / NAM rig (called from the editor / message thread).
    bool loadUserIr  (const juce::File& f) { return engine.loadUserIr (f); }
    bool loadUserRig (const juce::File& f) { return engine.loadUserRig (f); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    NamEngine::Params gatherParams();

    NamEngine engine;
    juce::AudioParameterBool* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApexAmpProcessor)
};
