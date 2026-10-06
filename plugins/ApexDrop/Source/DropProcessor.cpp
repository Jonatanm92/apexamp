#include "DropProcessor.h"

namespace pid
{
    constexpr auto bypass = "bypass";
    constexpr auto shift  = "shift";
    constexpr auto fine   = "fine";
    constexpr auto mode   = "mode";
    constexpr auto body   = "body";
    constexpr auto mix    = "mix";
    constexpr auto output = "output";
}

DropProcessor::DropProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (pid::bypass));
    shiftParam  = apvts.getRawParameterValue (pid::shift);
    fineParam   = apvts.getRawParameterValue (pid::fine);
    modeParam   = apvts.getRawParameterValue (pid::mode);
    bodyParam   = apvts.getRawParameterValue (pid::body);
    mixParam    = apvts.getRawParameterValue (pid::mix);
    outParam    = apvts.getRawParameterValue (pid::output);

    startTimerHz (10);
}

juce::AudioProcessorValueTreeState::ParameterLayout DropProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::bypass, 1 }, "Bypass", false));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { pid::shift, 1 }, "Shift", -24, 24, 0,
        AudioParameterIntAttributes().withLabel ("st")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::fine, 1 }, "Fine",
        NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("ct")));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::mode, 1 }, "Mode", StringArray { "Live", "Studio" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::body, 1 }, "Body",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::output, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

apex::dsp::PitchMode DropProcessor::currentModeParam() const
{
    return modeParam->load() > 0.5f ? apex::dsp::PitchMode::studio : apex::dsp::PitchMode::live;
}

void DropProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    shifter.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumOutputChannels()));
    shifter.setMode (currentModeParam());
    shifter.setSemitones ((float) shiftParam->load() + fineParam->load() * 0.01f);
    shifter.setBody (bodyParam->load() * 0.01f);
    shifter.setMix (bypassParam->get() ? 0.0f : mixParam->load() * 0.01f);
    shifter.reset();

    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outParam->load()));

    setLatencySamples (shifter.getLatencySamples());
}

bool DropProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void DropProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const auto mode = currentModeParam();
    if (mode != shifter.getMode())
    {
        shifter.setMode (mode);
        pendingLatency.store (shifter.getLatencySamples());
    }

    shifter.setSemitones ((float) shiftParam->load() + fineParam->load() * 0.01f);
    shifter.setBody (bodyParam->load() * 0.01f);

    // Bypass fades to the latency-aligned dry signal instead of cutting out,
    // so the host's delay compensation stays valid and nothing clicks.
    shifter.setMix (bypassParam->get() ? 0.0f : mixParam->load() * 0.01f);

    shifter.process (buffer.getArrayOfWritePointers(), juce::jmin (numOut, buffer.getNumChannels()),
                     buffer.getNumSamples());

    outputGain.setTargetValue (bypassParam->get() ? 1.0f : juce::Decibels::decibelsToGain (outParam->load()));
    outputGain.applyGain (buffer, buffer.getNumSamples());
}

void DropProcessor::timerCallback()
{
    const int latency = pendingLatency.exchange (-1);
    if (latency >= 0)
        setLatencySamples (latency);
}

juce::AudioProcessorEditor* DropProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void DropProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DropProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DropProcessor();
}
