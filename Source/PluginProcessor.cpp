#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    constexpr auto cabOn      = "cabOn";
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

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::cabOn, 1 }, "Cab", true));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::master, 1 }, "Master",
        NormalisableRange<float> (-36.0f, 12.0f, 0.1f), -3.0f));

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

    engine.setParams (gatherParams());
    engine.setCabEnabled (apvts.getRawParameterValue (pid::cabOn)->load() > 0.5f);
    engine.setMasterGainDb (apvts.getRawParameterValue (pid::master)->load());

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);
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
