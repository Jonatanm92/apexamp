#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSPCore.h"
#include "MainComponent.h"

namespace namapp {
namespace plugin {

class NamAppProcessor : public juce::AudioProcessor {
public:
    NamAppProcessor();
    ~NamAppProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NAM App"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    namapp::dsp::DSPCore mDspCore;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NamAppProcessor)
};

} // namespace plugin
} // namespace namapp
