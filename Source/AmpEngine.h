#pragma once

#include <juce_dsp/juce_dsp.h>
#include "dsp/AmpCore.h"
#include "dsp/CabSim.h"
#include "dsp/OutputStage.h"

namespace apex
{
/**
    JUCE-side engine that wraps the pure-C++ AmpCore with the bits that need the
    framework: oversampling around the nonlinear amp, and cabinet processing.

    - The amp (preamp + power amp nonlinearities) runs inside an N-times oversampled
      block so aliasing from the saturation is pushed well above the audible range
      before downsampling.
    - Cabinet stage runs at host rate (linear, no aliasing concern):
        * by default a smooth filter-based speaker voicing (CabSim) so the plugin
          sounds musical immediately with no IR loaded;
        * if the user loads a WAV/AIFF IR, partitioned convolution is used instead.
*/
class AmpEngine
{
public:
    AmpEngine() = default;

    void prepare (double sampleRate, int samplesPerBlock, int numChannels)
    {
        hostRate    = sampleRate;
        maxBlock    = samplesPerBlock;
        channels    = juce::jlimit (1, 2, numChannels);

        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) channels, oversampleFactorLog2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
        oversampling->initProcessing ((size_t) samplesPerBlock);

        const double osRate = sampleRate * (1 << oversampleFactorLog2);
        for (int ch = 0; ch < 2; ++ch)
        {
            cores[ch].prepare (osRate);
            cabSim[ch].prepare (sampleRate);
            outStage[ch].prepare (sampleRate);
        }

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                      (juce::uint32) channels };
        convolution.prepare (spec);

        // Re-load the user IR if one was set — convolution.prepare() clears it,
        // and the host can re-prepare us at any time (this was the bug where a
        // loaded IR seemed to "disappear" until something forced a reload).
        if (userIRFile.existsAsFile())
            loadIRInternal (userIRFile);

        outputGain.prepare (spec);
        outputGain.setRampDurationSeconds (0.02);

        reset();
    }

    void reset()
    {
        if (oversampling) oversampling->reset();
        for (int ch = 0; ch < 2; ++ch) { cores[ch].reset(); cabSim[ch].reset(); outStage[ch].reset(); }
        convolution.reset();
    }

    void setParams (const AmpParams& p)
    {
        for (int ch = 0; ch < 2; ++ch)
            cores[ch].setParams (p);
    }

    void setCabEnabled (bool shouldBeEnabled) noexcept { cabEnabled = shouldBeEnabled; }

    void setCabType (CabType t) noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
            cabSim[ch].setType (t);
    }

    void setMasterGainDb (float db) { outputGain.setGainDecibels (db); }

    void setOutputParams (float punch01, float loudDb) noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
            outStage[ch].setParams (punch01, loudDb);
    }

    /** Load a user IR from a WAV/AIFF file. Switches the cab to convolution mode. */
    void loadCabIRFromFile (const juce::File& file)
    {
        if (file.existsAsFile())
        {
            userIRFile = file;
            loadIRInternal (file);
            usingUserIR = true;
        }
    }

    /** Revert to the built-in filter cab. */
    void clearCabIR() noexcept
    {
        usingUserIR = false;
        userIRFile = juce::File();
    }

    juce::File getCabIRFile() const { return userIRFile; }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        // --- oversampled nonlinear amp ---
        auto osBlock = oversampling->processSamplesUp (block);

        const int numCh = (int) osBlock.getNumChannels();
        const int numS  = (int) osBlock.getNumSamples();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = osBlock.getChannelPointer ((size_t) ch);
            cores[juce::jmin (ch, 1)].process (data, numS);
        }

        oversampling->processSamplesDown (block);

        // --- cabinet stage at host rate ---
        if (cabEnabled)
        {
            if (usingUserIR)
            {
                juce::dsp::ProcessContextReplacing<float> ctx (block);
                convolution.process (ctx);
            }
            else
            {
                const int chs = (int) block.getNumChannels();
                const int n   = (int) block.getNumSamples();
                for (int ch = 0; ch < chs; ++ch)
                    cabSim[juce::jmin (ch, 1)].process (block.getChannelPointer ((size_t) ch), n);
            }
        }

        // --- output stage: punch + loud (no waveshaping, alias-free) ---
        {
            const int chs = (int) block.getNumChannels();
            const int n   = (int) block.getNumSamples();
            for (int ch = 0; ch < chs; ++ch)
                outStage[juce::jmin (ch, 1)].process (block.getChannelPointer ((size_t) ch), n);
        }

        juce::dsp::ProcessContextReplacing<float> gainCtx (block);
        outputGain.process (gainCtx);
    }

    int getLatencySamples() const
    {
        return oversampling ? (int) oversampling->getLatencyInSamples() : 0;
    }

private:
    void loadIRInternal (const juce::File& file)
    {
        convolution.loadImpulseResponse (file,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            0,
            juce::dsp::Convolution::Normalise::yes);
    }

    static constexpr int oversampleFactorLog2 = 3; // 8x — more headroom for high-gain, less aliasing

    double hostRate = 44100.0;
    int    maxBlock = 512;
    int    channels = 2;
    bool   cabEnabled = true;
    bool   usingUserIR = false;
    juce::File userIRFile;

    AmpCore cores[2];
    CabSim  cabSim[2];
    OutputStage outStage[2];
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::Convolution convolution;
    juce::dsp::Gain<float> outputGain;
};
} // namespace apex
