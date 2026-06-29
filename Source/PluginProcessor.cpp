#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace pid
{
    constexpr auto channel    = "channel";
    constexpr auto tonestack  = "tonestack";
    constexpr auto inputTrim  = "inputTrim";
    constexpr auto gain       = "gain";
    constexpr auto push       = "push";
    constexpr auto tight      = "tight";
    constexpr auto superCut   = "superCut";
    constexpr auto bias       = "bias";
    constexpr auto bass       = "bass";
    constexpr auto mid        = "mid";
    constexpr auto treble     = "treble";
    constexpr auto chug       = "chug";
    constexpr auto lowDirtDrv = "lowDirtDrive";
    constexpr auto lowDirtMix = "lowDirtMix";
    constexpr auto sag        = "sag";
    constexpr auto powerDrive = "powerDrive";
    constexpr auto gate       = "gate";
    constexpr auto boostOn    = "boostOn";
    constexpr auto boostDrive = "boostDrive";
    constexpr auto boostTone  = "boostTone";
    constexpr auto cabOn      = "cabOn";
    constexpr auto cabType    = "cabType";
    constexpr auto cabBlend   = "cabBlend";
    constexpr auto outPunch   = "outPunch";
    constexpr auto outLoud    = "outLoud";
    constexpr auto autoTight  = "autoTight";
    constexpr auto width      = "width";
    constexpr auto whammyOn   = "whammyOn";
    constexpr auto whammyShift= "whammyShift";
    constexpr auto whammyMix  = "whammyMix";
    constexpr auto master     = "master";
}

ApexAmpProcessor::ApexAmpProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ApexAmpProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float def) {
        return NormalisableRange<float> (0.0f, 1.0f, 0.001f);
    };

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::channel, 1 }, "Channel",
        StringArray { "Tight", "Scoop" }, 0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::tonestack, 1 }, "Tonestack",
        StringArray { "Marshall", "Fender", "Mesa", "Modern Metal" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::inputTrim, 1 }, "Input Trim",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::gain, 1 },     "Gain",      pct (0.5f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::push, 1 },     "Push",      pct (0.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::tight, 1 },    "Tight",     pct (0.3f), 0.3f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::superCut, 1 }, "Super Cut", pct (0.0f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::bias, 1 }, "Bias",
        NormalisableRange<float> (-0.1f, 0.1f, 0.001f), 0.02f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::bass, 1 },   "Bass",   pct (0.5f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::mid, 1 },    "Mid",    pct (0.5f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::treble, 1 }, "Treble", pct (0.5f), 0.5f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::chug, 1 },       "Chug",          pct (0.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::lowDirtDrv, 1 }, "Low Dirt Drive",pct (0.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::lowDirtMix, 1 }, "Low Dirt Mix",  pct (0.0f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::sag, 1 },        "Sag",         pct (0.3f), 0.3f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::powerDrive, 1 }, "Power Drive", pct (0.3f), 0.3f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::gate, 1 }, "Gate",
        NormalisableRange<float> (-80.0f, -20.0f, 0.5f), -60.0f));

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { pid::boostOn, 1 },    "Boost", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::boostDrive, 1 }, "Boost Drive", pct (0.5f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::boostTone, 1 },  "Boost Tone",  pct (0.5f), 0.5f));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::cabOn, 1 }, "Cab", true));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::cabType, 1 }, "Cab Type",
        StringArray { "Modern V30", "Vintage Greenback", "Tight 4x12", "American Scooped" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::cabBlend, 1 }, "IR Blend",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::outPunch, 1 }, "Punch",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::outLoud, 1 }, "Loud",
        NormalisableRange<float> (0.0f, 12.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::autoTight, 1 }, "Auto Tight", pct (0.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::width, 1 },     "Width",      pct (0.0f), 0.0f));

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { pid::whammyOn, 1 }, "Whammy", false));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::whammyShift, 1 }, "Whammy Shift",
        NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 12.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::whammyMix, 1 }, "Whammy Mix", pct (1.0f), 1.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::master, 1 }, "Master",
        NormalisableRange<float> (-36.0f, 12.0f, 0.1f), -6.0f));

    return layout;
}

apex::AmpParams ApexAmpProcessor::gatherParams()
{
    apex::AmpParams p;
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    p.channel   = (int) get (pid::channel) == 0 ? apex::PreampChannel::tight
                                                : apex::PreampChannel::scoop;
    p.tonestack = (apex::TonestackModel) (int) get (pid::tonestack);

    p.inputTrimDb = get (pid::inputTrim);
    p.gain     = get (pid::gain);
    p.push     = get (pid::push);
    p.tight    = get (pid::tight);
    p.superCut = get (pid::superCut);
    p.bias     = get (pid::bias);

    p.bass   = get (pid::bass);
    p.mid    = get (pid::mid);
    p.treble = get (pid::treble);

    p.chug         = get (pid::chug);
    p.lowDirtDrive = get (pid::lowDirtDrv);
    p.lowDirtMix   = get (pid::lowDirtMix);

    p.sag        = get (pid::sag);
    p.powerDrive = get (pid::powerDrive);
    p.gateThresholdDb = get (pid::gate);

    p.boostOn    = get (pid::boostOn) > 0.5f;
    p.boostDrive = get (pid::boostDrive);
    p.boostTone  = get (pid::boostTone);

    p.autoTight  = get (pid::autoTight);
    p.trackedHz  = pitchDetector.getFrequency(); // feed the tuner pitch into adaptive tightness

    return p;
}

void ApexAmpProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    pitchDetector.prepare (sampleRate, samplesPerBlock);
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
    const int numSamples = buffer.getNumSamples();

    // --- collapse input to mono ---------------------------------------------
    // A guitar is a mono source, and it may be plugged into ANY physical input
    // (e.g. only input 2 on the interface). Summing all input channels into
    // channel 0 means the amp always "hears" the guitar regardless of which
    // input it is on, and avoids the hard-panned / half-silent sound you get
    // when a mono guitar is fed into a stereo path.
    if (totalIn > 1)
    {
        auto* dst = buffer.getWritePointer (0);
        for (int ch = 1; ch < totalIn; ++ch)
        {
            const auto* src = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                dst[i] += src[i];
        }
    }

    // Mirror the mono signal to every output channel (centred, dual-mono out).
    for (int ch = 1; ch < totalOut; ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

    // Feed the tuner from the dry mono DI (before the amp processes it) and
    // measure the input level for the meter.
    float inPk = 0.0f;
    if (buffer.getNumChannels() > 0)
    {
        const float* di = buffer.getReadPointer (0);
        for (int i = 0; i < numSamples; ++i)
        {
            pitchDetector.pushSample (di[i]);
            inPk = juce::jmax (inPk, std::abs (di[i]));
        }
    }
    inLevel.store (inPk);

    engine.setParams (gatherParams());
    engine.setCabEnabled (apvts.getRawParameterValue (pid::cabOn)->load() > 0.5f);
    engine.setCabType ((apex::CabType) (int) apvts.getRawParameterValue (pid::cabType)->load());
    engine.setCabBlend (apvts.getRawParameterValue (pid::cabBlend)->load());
    engine.setOutputParams (apvts.getRawParameterValue (pid::outPunch)->load(),
                            apvts.getRawParameterValue (pid::outLoud)->load());
    engine.setWidth (apvts.getRawParameterValue (pid::width)->load());
    engine.setWhammy (apvts.getRawParameterValue (pid::whammyOn)->load() > 0.5f,
                      apvts.getRawParameterValue (pid::whammyShift)->load(),
                      apvts.getRawParameterValue (pid::whammyMix)->load());
    engine.setMasterGainDb (apvts.getRawParameterValue (pid::master)->load());

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);

    // Output safety + level metering: scrub any non-finite samples (so a bad IR
    // or extreme setting can never blast NaNs/garbage to the speakers) and track
    // the output peak for the meter.
    float outPk = 0.0f;
    for (int ch = 0; ch < totalOut; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float v = d[i];
            if (! std::isfinite (v)) { v = 0.0f; d[i] = 0.0f; }
            outPk = juce::jmax (outPk, std::abs (v));
        }
    }
    outLevel.store (outPk);
    tunerFreq.store (pitchDetector.getFrequency());
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
    {
        apvts.replaceState (tree);

        // Restore previously loaded cab IRs, if any.
        const auto path = apvts.state.getProperty ("irPath", "").toString();
        if (path.isNotEmpty())
        {
            const juce::File f (path);
            if (f.existsAsFile())
                engine.loadCabIRFromFile (f);
        }
        const auto pathB = apvts.state.getProperty ("irPathB", "").toString();
        if (pathB.isNotEmpty())
        {
            const juce::File f (pathB);
            if (f.existsAsFile())
                engine.loadCabIRBFromFile (f);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ApexAmpProcessor();
}
