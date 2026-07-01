#include "PluginProcessor.h"
#include "ApexEditor.h"

namespace pid
{
    constexpr auto inputGain  = "inputGain";
    constexpr auto outputGain = "outputGain";
    constexpr auto tight      = "tight";
    constexpr auto boostOn    = "boostOn";
    constexpr auto boostDrive = "boostDrive";
    constexpr auto boostTone  = "boostTone";
    constexpr auto boostLevel = "boostLevel";
    constexpr auto rigMode    = "rigMode";
    constexpr auto rig        = "rig";
    constexpr auto mixBite    = "mixBite";
    constexpr auto mixBody    = "mixBody";
    constexpr auto mixEdge    = "mixEdge";
    constexpr auto cabMix     = "cabMix";
    constexpr auto ir         = "ir";
    constexpr auto gateOn     = "gateOn";
    constexpr auto gate       = "gate";
    constexpr auto gateHold   = "gateHold";
    constexpr auto lowCut     = "lowCut";
    constexpr auto presence   = "presence";
}

ApexAmpProcessor::ApexAmpProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

juce::AudioProcessorValueTreeState::ParameterLayout ApexAmpProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "bypass", 1 }, "Bypass", false));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::inputGain, 1 }, "Input Gain",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::outputGain, 1 }, "Output Gain",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::tight, 1 }, "Tight",
        NormalisableRange<float> (20.0f, 300.0f, 1.0f, 0.5f), 20.0f));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::boostOn, 1 }, "Boost", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::boostDrive, 1 }, "Boost Drive",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::boostTone, 1 }, "Boost Tone",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::boostLevel, 1 }, "Boost Level",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::rigMode, 1 }, "Rig Mode",
        StringArray { "Single", "Blend" }, 0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::rig, 1 }, "Rig",
        StringArray { "Bite", "Body", "Edge", "User" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::mixBite, 1 }, "Blend: Bite",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::mixBody, 1 }, "Blend: Body",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::mixEdge, 1 }, "Blend: Edge",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::cabMix, 1 }, "Cab Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::ir, 1 }, "Cabinet IR",
        StringArray { "Ashen", "Meshuggah", "PDI-09", "User" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::gateOn, 1 }, "Gate", false));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::gate, 1 }, "Gate Threshold",
        NormalisableRange<float> (-80.0f, -20.0f, 0.5f), -60.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::gateHold, 1 }, "Gate Hold",
        NormalisableRange<float> (10.0f, 500.0f, 1.0f, 0.5f), 50.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::lowCut, 1 }, "Low Cut",
        NormalisableRange<float> (20.0f, 300.0f, 1.0f, 0.5f), 80.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::presence, 1 }, "Presence",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return layout;
}

NamEngine::Params ApexAmpProcessor::gatherParams()
{
    NamEngine::Params p;
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    p.inputGainDb  = get (pid::inputGain);
    p.outputGainDb = get (pid::outputGain);
    p.tightHz      = get (pid::tight);
    p.boostOn      = get (pid::boostOn) > 0.5f;
    p.boostDrive   = get (pid::boostDrive) * 0.01f;
    p.boostTone    = get (pid::boostTone) * 0.01f;
    p.boostLevelDb = get (pid::boostLevel);
    p.rigMode      = ((int) get (pid::rigMode) == 1) ? NamEngine::RigMode::Blend
                                                     : NamEngine::RigMode::Single;
    p.singleIndex  = (int) get (pid::rig);
    p.mix[0]       = get (pid::mixBite);
    p.mix[1]       = get (pid::mixBody);
    p.mix[2]       = get (pid::mixEdge);
    p.cabMix       = juce::jlimit (0.0f, 1.0f, get (pid::cabMix) * 0.01f);
    p.irIndex      = (int) get (pid::ir);
    p.gateEnabled  = get (pid::gateOn) > 0.5f;
    p.gateThreshDb = get (pid::gate);
    p.gateHoldMs   = get (pid::gateHold);
    p.lowCutHz     = get (pid::lowCut);
    p.presenceDb   = get (pid::presence);

    return p;
}

void ApexAmpProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (engine.getLatencySamples());
}

bool ApexAmpProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ApexAmpProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();

    // Clear any output-only channels.
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int n = buffer.getNumSamples();

    // Input level (pre-engine).
    float inMag = 0.0f;
    for (int ch = 0; ch < juce::jmin (totalIn, buffer.getNumChannels()); ++ch)
        inMag = juce::jmax (inMag, buffer.getMagnitude (ch, 0, n));
    inputMagnitude.store (inMag);

    if (bypassParam == nullptr || ! bypassParam->get())
        engine.process (buffer, gatherParams());

    // Output level (post-engine).
    float outMag = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        outMag = juce::jmax (outMag, buffer.getMagnitude (ch, 0, n));
    outputMagnitude.store (outMag);
}

juce::AudioProcessorEditor* ApexAmpProcessor::createEditor()
{
    return new ApexAmpEditor (*this);
}

void ApexAmpProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream mos (destData, true);
        state.writeToStream (mos);
    }
}

void ApexAmpProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ApexAmpProcessor();
}
