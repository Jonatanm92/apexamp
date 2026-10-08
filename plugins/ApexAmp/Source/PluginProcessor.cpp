#include "PluginProcessor.h"
#include "AmpEditor.h"
#include "Presets.h"

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
    constexpr auto dropOn     = "dropOn";
    constexpr auto dropShift  = "dropShift";
    constexpr auto dropBody   = "dropBody";
    constexpr auto dropSub    = "dropSub";
    constexpr auto bass       = "bass";
    constexpr auto mid        = "mid";
    constexpr auto treble     = "treble";
    constexpr auto inputTrim  = "inputTrim";
    constexpr auto shapeOn    = "shapeOn";
    constexpr auto chug       = "chug";
    constexpr auto chugFreq   = "chugFreq";
    constexpr auto dirt       = "dirt";
    constexpr auto delayOn    = "delayOn";
    constexpr auto delayTime  = "delayTime";
    constexpr auto delaySync  = "delaySync";
    constexpr auto delayDiv   = "delayDiv";
    constexpr auto delayFeedback = "delayFeedback";
    constexpr auto delayDuck  = "delayDuck";
    constexpr auto delayMix   = "delayMix";
    constexpr auto reverbOn   = "reverbOn";
    constexpr auto reverbDecay = "reverbDecay";
    constexpr auto reverbAbyss = "reverbAbyss";
    constexpr auto reverbTone = "reverbTone";
    constexpr auto reverbMix  = "reverbMix";
}

namespace
{
    // Echo divisions in quarter notes, matching the delayDiv choices.
    constexpr double divisionBeats[] = { 2.0, 1.5, 1.0, 2.0 / 3.0, 0.75, 0.5, 1.0 / 3.0, 0.25 };

    // Auto Input: where the 90th percentile of 10 ms peaks should land.
    constexpr float autoTargetDb = -9.0f;
    constexpr float autoFloorDb  = -54.0f;
}

ApexAmpProcessor::ApexAmpProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMETERS", createLayout()),
      presets (apvts, "ApexAmp", ApexPresets::all(), { "bypass", pid::inputTrim })
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
    dropOnParam = apvts.getRawParameterValue (pid::dropOn);
    autoHistogram.fill (0);
    startTimerHz (10);
}

ApexAmpProcessor::~ApexAmpProcessor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout ApexAmpProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "bypass", 1 }, "Bypass", false));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::inputGain, 1 }, "Input Gain",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::outputGain, 1 }, "Output Gain",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::tight, 1 }, "Tight",
        NormalisableRange<float> (20.0f, 300.0f, 1.0f, 0.5f), 20.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::boostOn, 1 }, "Boost", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::boostDrive, 1 }, "Boost Drive",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::boostTone, 1 }, "Boost Tone",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::boostLevel, 1 }, "Boost Level",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));

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
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::ir, 1 }, "Cabinet IR",
        StringArray { "Ashen", "Meshuggah", "PDI-09", "User" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::gateOn, 1 }, "Gate", false));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::gate, 1 }, "Gate Threshold",
        NormalisableRange<float> (-80.0f, -20.0f, 0.5f), -60.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::gateHold, 1 }, "Gate Hold",
        NormalisableRange<float> (10.0f, 500.0f, 1.0f, 0.5f), 50.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::lowCut, 1 }, "Low Cut",
        NormalisableRange<float> (20.0f, 300.0f, 1.0f, 0.5f), 80.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::presence, 1 }, "Presence",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    // ---- Drop pedal (v0.6) ---------------------------------------------------
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::dropOn, 1 }, "Drop", false));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { pid::dropShift, 1 }, "Drop Shift", -12, 12, -5,
        AudioParameterIntAttributes().withLabel ("st")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::dropBody, 1 }, "Drop Body",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::dropSub, 1 }, "Drop Sub",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("%")));

    // ---- Amp EQ (v0.6), flat at 0 dB -------------------------------------------
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::bass, 1 }, "Bass",
        NormalisableRange<float> (-10.0f, 10.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::mid, 1 }, "Mid",
        NormalisableRange<float> (-10.0f, 10.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::treble, 1 }, "Treble",
        NormalisableRange<float> (-10.0f, 10.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));

    // ---- v0.7: Auto Input trim, Shape pedal, Void unit (all off by default) -----
    auto skewed = [] (float lo, float hi, float step, float centre)
    {
        NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    };
    auto percent = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), def, AudioParameterFloatAttributes().withLabel ("%"));
    };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::inputTrim, 1 }, "Input Trim",
        NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::shapeOn, 1 }, "Shape", false));
    layout.add (percent (pid::chug, "Chug", 50.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::chugFreq, 1 }, "Chug Frequency",
        skewed (100.0f, 4000.0f, 1.0f, 630.0f), 700.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (percent (pid::dirt, "Low Dirt", 25.0f));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::delayOn, 1 }, "Echo", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::delayTime, 1 }, "Echo Time",
        skewed (20.0f, 2000.0f, 1.0f, 300.0f), 375.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::delaySync, 1 }, "Echo Sync", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::delayDiv, 1 }, "Echo Division",
        StringArray { "1/2", "1/4.", "1/4", "1/4T", "1/8.", "1/8", "1/8T", "1/16" }, 4));
    layout.add (percent (pid::delayFeedback, "Echo Feedback", 35.0f));
    layout.add (percent (pid::delayDuck, "Echo Duck", 0.0f));
    layout.add (percent (pid::delayMix, "Echo Mix", 30.0f));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::reverbOn, 1 }, "Abyss", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::reverbDecay, 1 }, "Abyss Decay",
        skewed (0.3f, 30.0f, 0.01f, 3.0f), 3.5f, AudioParameterFloatAttributes().withLabel ("s")));
    layout.add (percent (pid::reverbAbyss, "Abyss Depth", 25.0f));
    layout.add (percent (pid::reverbTone, "Abyss Tone", 50.0f));
    layout.add (percent (pid::reverbMix, "Abyss Mix", 25.0f));

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
    p.dropOn       = get (pid::dropOn) > 0.5f;
    p.dropShift    = get (pid::dropShift);
    p.dropBody     = get (pid::dropBody) * 0.01f;
    p.dropSub      = get (pid::dropSub) * 0.01f;
    p.bassDb       = get (pid::bass);
    p.midDb        = get (pid::mid);
    p.trebleDb     = get (pid::treble);
    p.inputTrimDb  = get (pid::inputTrim);

    p.shapeOn      = get (pid::shapeOn) > 0.5f;
    p.chug         = get (pid::chug) * 0.01f;
    p.chugFreqHz   = get (pid::chugFreq);
    p.dirt         = get (pid::dirt) * 0.01f;

    p.delayOn      = get (pid::delayOn) > 0.5f;
    p.delayMs      = getEchoMs();
    p.delayFeedback = get (pid::delayFeedback) * 0.01f;
    p.delayDuck    = get (pid::delayDuck) * 0.01f;
    p.delayMix     = get (pid::delayMix) * 0.01f;

    p.reverbOn     = get (pid::reverbOn) > 0.5f;
    p.reverbDecay  = get (pid::reverbDecay);
    p.reverbAbyss  = get (pid::reverbAbyss) * 0.01f;
    p.reverbTone   = get (pid::reverbTone) * 0.01f;
    p.reverbMix    = get (pid::reverbMix) * 0.01f;

    return p;
}

double ApexAmpProcessor::getEchoMs() const
{
    if (apvts.getRawParameterValue (pid::delaySync)->load() < 0.5f)
        return apvts.getRawParameterValue (pid::delayTime)->load();

    const int division = juce::jlimit (0, 7, (int) apvts.getRawParameterValue (pid::delayDiv)->load());
    return juce::jmin (2000.0, 60000.0 / hostBpm.load() * divisionBeats[division]);
}

double ApexAmpProcessor::getTailLengthSeconds() const
{
    double tail = 0.2;
    if (apvts.getRawParameterValue (pid::delayOn)->load() > 0.5f)
    {
        // repeats fall 60 dB after log(0.001) / log(feedback) passes
        const double fb = juce::jlimit (0.01, 0.95, 0.95 * apvts.getRawParameterValue (pid::delayFeedback)->load() * 0.01);
        tail += 0.001 * getEchoMs() * std::ceil (std::log (0.001) / std::log (fb));
    }
    if (apvts.getRawParameterValue (pid::reverbOn)->load() > 0.5f)
        tail += 1.2 * apvts.getRawParameterValue (pid::reverbDecay)->load();
    return juce::jmin (tail, 60.0);
}

void ApexAmpProcessor::startAutoInput()
{
    autoOutcome.store (0);
    autoLearning.store (true);
}

void ApexAmpProcessor::listenForAutoInput (const juce::AudioBuffer<float>& buffer, int channels, int n)
{
    if (! autoLearning.load (std::memory_order_relaxed))
    {
        autoRunning = false;
        return;
    }
    if (! autoRunning)
    {
        autoHistogram.fill (0);
        autoPlayingSamples = autoTotalSamples = autoWindowCount = 0;
        autoWindowPeak = 0.0f;
        autoRunning = true;
    }

    // Peaks over 10 ms windows (independent of the host block size); the ones
    // above the noise floor count as playing.
    const int window = juce::jmax (1, (int) (0.01 * getSampleRate()));
    for (int i = 0; i < n; ++i)
    {
        float v = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
            v = juce::jmax (v, std::abs (buffer.getReadPointer (ch)[i]));
        autoWindowPeak = juce::jmax (autoWindowPeak, v);
        if (++autoWindowCount >= window)
        {
            const float db = juce::Decibels::gainToDecibels (autoWindowPeak, -100.0f);
            if (db > autoFloorDb)
            {
                const int bin = juce::jlimit (0, (int) autoHistogram.size() - 1, (int) std::lround ((db + 60.0f) * 2.0f));
                ++autoHistogram[(size_t) bin];
                autoPlayingSamples += window;
            }
            autoWindowPeak = 0.0f;
            autoWindowCount = 0;
        }
    }
    autoTotalSamples += n;

    const double rate = getSampleRate();
    if (autoPlayingSamples < (int) (3.0 * rate) && autoTotalSamples < (int) (10.0 * rate))
        return;

    if (autoPlayingSamples >= (int) (0.75 * rate))
    {
        int total = 0;
        for (int c : autoHistogram) total += c;
        int below = 0, bin = 0;
        for (; bin < (int) autoHistogram.size(); ++bin)
            if ((below += autoHistogram[(size_t) bin]) >= (int) std::ceil (0.9 * total))
                break;
        const float peakDb = (float) bin * 0.5f - 60.0f;
        autoTrimResult.store (juce::jlimit (-18.0f, 18.0f, autoTargetDb - peakDb));
        autoOutcome.store (1);
    }
    else
    {
        autoOutcome.store (2);
    }
    autoLearning.store (false);
    autoRunning = false;
}

int ApexAmpProcessor::currentLatency() const
{
    return engine.getLatencySamples() + (dropOnParam->load() > 0.5f ? engine.getDropLatencySamples() : 0);
}

void ApexAmpProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    tunerFeed.prepare (sampleRate);
    reportedLatency = currentLatency();
    setLatencySamples (reportedLatency);
}

void ApexAmpProcessor::timerCallback()
{
    // Auto Input finished on the audio thread: apply the trim as one undoable step.
    if (autoOutcome.load() == 1 && ! autoTrimApplied)
    {
        auto* trim = apvts.getParameter (pid::inputTrim);
        undoManager.beginNewTransaction ("Auto Input");
        trim->beginChangeGesture();
        trim->setValueNotifyingHost (trim->convertTo0to1 (autoTrimResult.load()));
        trim->endChangeGesture();
        autoTrimApplied = true;
    }
    if (autoOutcome.load() == 0)
        autoTrimApplied = false;

    // The Drop pedal adds the pitch engine's latency; tell the host from the
    // message thread whenever it is switched.
    const int latency = currentLatency();
    if (latency != reportedLatency)
    {
        reportedLatency = latency;
        setLatencySamples (latency);
    }
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
    tunerFeed.push (buffer.getArrayOfReadPointers(), juce::jmin (totalIn, buffer.getNumChannels()), n);
    listenForAutoInput (buffer, juce::jmin (totalIn, buffer.getNumChannels()), n);

    if (auto* head = getPlayHead())
        if (auto position = head->getPosition())
            if (auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 20.0 && *bpm < 400.0)
                hostBpm.store (*bpm, std::memory_order_relaxed);

    if (bypassParam == nullptr || ! bypassParam->get())
        engine.process (buffer, gatherParams());

    if (tunerFeed.mute.load (std::memory_order_relaxed))
        buffer.clear();

    // Output level (post-engine).
    float outMag = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        outMag = juce::jmax (outMag, buffer.getMagnitude (ch, 0, n));
    outputMagnitude.store (outMag);
}

juce::AudioProcessorEditor* ApexAmpProcessor::createEditor()
{
    return new AmpEditor (*this);
}

void ApexAmpProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        presets.writeTo (state);
        juce::MemoryOutputStream mos (destData, true);
        state.writeToStream (mos);
    }
}

void ApexAmpProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState (tree);
        presets.readFrom (tree);
        undoManager.clearUndoHistory();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ApexAmpProcessor();
}
