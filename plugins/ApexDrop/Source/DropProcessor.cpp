#include "DropProcessor.h"
#include "DropEditor.h"

namespace pid
{
    constexpr auto bypass = "bypass";
    constexpr auto shift  = "shift";
    constexpr auto fine   = "fine";
    constexpr auto mode   = "mode";
    constexpr auto body   = "body";
    constexpr auto sub    = "sub";
    constexpr auto mix    = "mix";
    constexpr auto output = "output";
}

DropProcessor::DropProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMETERS", createLayout()),
      presets (apvts, "Apex Drop", factoryPresets(), { pid::bypass })
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (pid::bypass));
    shiftParam  = apvts.getRawParameterValue (pid::shift);
    fineParam   = apvts.getRawParameterValue (pid::fine);
    modeParam   = apvts.getRawParameterValue (pid::mode);
    bodyParam   = apvts.getRawParameterValue (pid::body);
    subParam    = apvts.getRawParameterValue (pid::sub);
    mixParam    = apvts.getRawParameterValue (pid::mix);
    outParam    = apvts.getRawParameterValue (pid::output);

    startTimerHz (10);
}

DropProcessor::~DropProcessor()
{
    stopTimer();
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

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::sub, 1 }, "Sub",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    return layout;
}

std::vector<apex::ui::FactoryPreset> DropProcessor::factoryPresets()
{
    return {
        { "Init",             { { pid::shift, 0.0f } } },
        { "Drop D",           { { pid::shift, -2.0f } } },
        { "C Standard",       { { pid::shift, -4.0f } } },
        { "B Standard",       { { pid::shift, -5.0f } } },
        { "A Standard",       { { pid::shift, -7.0f } } },
        { "Thall - F#",       { { pid::shift, -10.0f }, { pid::sub, 30.0f } } },
        { "Octave Down",      { { pid::shift, -12.0f } } },
        { "Thicken (Sub)",    { { pid::shift, 0.0f }, { pid::sub, 45.0f } } },
        { "Studio - A Std",   { { pid::shift, -7.0f }, { pid::mode, 1.0f } } },
        { "Classic Shifter",  { { pid::shift, -5.0f }, { pid::body, 0.0f } } },
    };
}

apex::dsp::PitchMode DropProcessor::currentModeParam() const
{
    return modeParam->load() > 0.5f ? apex::dsp::PitchMode::studio : apex::dsp::PitchMode::live;
}

double DropProcessor::getLatencyMs (apex::dsp::PitchMode mode) const
{
    return 1000.0 * shifter.getLatencySamples (mode) / currentRate;
}

void DropProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    const int channels = juce::jmax (1, getTotalNumOutputChannels());
    shifter.prepare (sampleRate, samplesPerBlock, channels);
    subShifter.prepare (sampleRate, samplesPerBlock, channels);
    subBuffer.setSize (channels, samplesPerBlock);

    for (auto* s : { &shifter, &subShifter })
    {
        s->setMode (currentModeParam());
        s->setBody (bodyParam->load() * 0.01f);
    }
    shifter.setSemitones ((float) shiftParam->load() + fineParam->load() * 0.01f);
    subShifter.setSemitones ((float) shiftParam->load() - 12.0f + fineParam->load() * 0.01f);
    shifter.setMix (bypassParam->get() ? 0.0f : mixParam->load() * 0.01f);
    subShifter.setMix (1.0f);
    shifter.reset();
    subShifter.reset();

    const float a = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 240.0f / (float) sampleRate);
    for (auto& f : subFilter)
        f = { a, 0.0f, 0.0f };

    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outParam->load()));
    subLevel.reset (sampleRate, 0.03);
    subLevel.setCurrentAndTargetValue (subParam->load() * 0.01f);

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
    const int n = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    const int channels = juce::jmin (numOut, buffer.getNumChannels());
    float inMag = 0.0f;
    for (int ch = 0; ch < channels; ++ch)
        inMag = juce::jmax (inMag, buffer.getMagnitude (ch, 0, n));
    inputMagnitude.store (inMag);

    const auto mode = currentModeParam();
    if (mode != shifter.getMode())
    {
        shifter.setMode (mode);
        subShifter.setMode (mode);
        pendingLatency.store (shifter.getLatencySamples());
    }

    const float semis = (float) shiftParam->load() + fineParam->load() * 0.01f;
    const float body = bodyParam->load() * 0.01f;
    shifter.setSemitones (semis);
    shifter.setBody (body);

    // Bypass fades to the latency-aligned dry signal instead of cutting out,
    // so the host's delay compensation stays valid and nothing clicks.
    const bool bypassed = bypassParam->get();
    const float mix = bypassed ? 0.0f : mixParam->load() * 0.01f;
    shifter.setMix (mix);

    // Sub-octave layer: shifted a further octave down, low-passed, added to the wet path.
    subLevel.setTargetValue (bypassed ? 0.0f : subParam->load() * 0.01f * mix);
    const bool subActive = subLevel.getCurrentValue() > 0.0f || subLevel.getTargetValue() > 0.0f;
    if (subActive && n <= subBuffer.getNumSamples())
    {
        for (int ch = 0; ch < channels; ++ch)
            subBuffer.copyFrom (ch, 0, buffer, ch, 0, n);
        subShifter.setSemitones (semis - 12.0f);
        subShifter.setBody (body);
        subShifter.process (subBuffer.getArrayOfWritePointers(), channels, n);
    }

    shifter.process (buffer.getArrayOfWritePointers(), channels, n);

    if (subActive && n <= subBuffer.getNumSamples())
    {
        for (int i = 0; i < n; ++i)
        {
            const float level = subLevel.getNextValue() * 1.4f;
            for (int ch = 0; ch < channels; ++ch)
                buffer.getWritePointer (ch)[i] += subFilter[ch].process (subBuffer.getSample (ch, i)) * level;
        }
    }

    outputGain.setTargetValue (bypassed ? 1.0f : juce::Decibels::decibelsToGain (outParam->load()));
    outputGain.applyGain (buffer, n);

    float outMag = 0.0f;
    for (int ch = 0; ch < channels; ++ch)
        outMag = juce::jmax (outMag, buffer.getMagnitude (ch, 0, n));
    outputMagnitude.store (outMag);
}

void DropProcessor::timerCallback()
{
    const int latency = pendingLatency.exchange (-1);
    if (latency >= 0)
        setLatencySamples (latency);
}

juce::AudioProcessorEditor* DropProcessor::createEditor()
{
    return new DropEditor (*this);
}

void DropProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    presets.writeTo (state);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DropProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            presets.readFrom (tree);
            undoManager.clearUndoHistory();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DropProcessor();
}
