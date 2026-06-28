#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "AmpEngine.h"

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

    const juce::String getName() const override { return "ApexAmp"; }
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

    /** Called by the editor when the user picks an IR file. */
    void loadCabIR (const juce::File& file)
    {
        engine.loadCabIRFromFile (file);
        apvts.state.setProperty ("irPath", file.getFullPathName(), nullptr);
    }

    /** Revert to the built-in filter cab. */
    void clearCabIR()
    {
        engine.clearCabIR();
        apvts.state.setProperty ("irPath", "", nullptr);
    }

    /** Currently loaded IR file (or an empty File if using the built-in cab). */
    juce::File getCabIRFile() const { return engine.getCabIRFile(); }

    /** Save/load a full preset (parameters + IR path) to a .apreset file. */
    void savePresetToFile (const juce::File& file)
    {
        juce::MemoryBlock mb;
        getStateInformation (mb);
        file.replaceWithData (mb.getData(), mb.getSize());
    }

    void loadPresetFromFile (const juce::File& file)
    {
        juce::MemoryBlock mb;
        if (file.loadFileAsData (mb))
            setStateInformation (mb.getData(), (int) mb.getSize());
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    apex::AmpParams gatherParams();

    apex::AmpEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApexAmpProcessor)
};
