#include "PluginProcessor.h"
#include "MainComponent.h" // We will reuse the MainComponent for the plugin GUI

namespace namapp {
namespace plugin {

NamAppProcessor::NamAppProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {
}

NamAppProcessor::~NamAppProcessor() {}

void NamAppProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    mDspCore.prepareToPlay(sampleRate, samplesPerBlock);
}

void NamAppProcessor::releaseResources() {
}

void NamAppProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    auto numSamples = buffer.getNumSamples();
    std::vector<float> processBuffer(numSamples);

    auto* inL = buffer.getReadPointer(0);
    auto* inR = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : inL;

    // Convert to Mono for Guitar DSP
    for (int i = 0; i < numSamples; ++i) {
        processBuffer[i] = (inL[i] + inR[i]) * 0.5f;
    }

    // Process DSP
    mDspCore.processBlock(processBuffer, processBuffer);

    // Output to Stereo
    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : outL;
    
    for (int i = 0; i < numSamples; ++i) {
        outL[i] = processBuffer[i];
        if (buffer.getNumChannels() > 1) {
            outR[i] = processBuffer[i];
        }
    }
}

juce::AudioProcessorEditor* NamAppProcessor::createEditor() {
    // In a real plugin, we would pass reference to mDspCore
    // For this prototype, we're returning the MainComponent wrapped in a GenericAudioProcessorEditor or directly modifying MainComponent to accept it.
    // To keep it simple for the build:
    return new juce::GenericAudioProcessorEditor(*this);
}

// JUCE Plugin Creation macro
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new NamAppProcessor();
}

} // namespace plugin
} // namespace namapp
