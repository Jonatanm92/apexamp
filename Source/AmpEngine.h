#pragma once

#include <juce_dsp/juce_dsp.h>
#include "dsp/AmpCore.h"
#include "dsp/CabSim.h"
#include "dsp/OutputStage.h"
#include "dsp/Doubler.h"
#include "dsp/PitchShifter.h"

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
        doubler.prepare (sampleRate);
        whammy.prepare (sampleRate);

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                      (juce::uint32) channels };
        convolution.prepare (spec);
        convolutionB.prepare (spec);

        // Re-load the user IRs if set — convolution.prepare() clears them, and the
        // host can re-prepare us at any time (this was the bug where a loaded IR
        // seemed to "disappear" until something forced a reload).
        if (userIRFile.existsAsFile())
            loadIRInternal (convolution, userIRFile);
        if (userIRFileB.existsAsFile())
            loadIRInternal (convolutionB, userIRFileB);

        scratch.setSize (channels, samplesPerBlock);

        outputGain.prepare (spec);
        outputGain.setRampDurationSeconds (0.02);

        reset();
    }

    void reset()
    {
        if (oversampling) oversampling->reset();
        for (int ch = 0; ch < 2; ++ch) { cores[ch].reset(); cabSim[ch].reset(); outStage[ch].reset(); }
        doubler.reset();
        whammy.reset();
        convolution.reset();
        convolutionB.reset();
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

    void setWidth (float w) noexcept { doubler.setWidth (w); }

    void setWhammy (bool on, float semitones, float mix) noexcept
    {
        whammy.setParams (on, semitones, mix);
    }

    /** Load a user IR from a WAV/AIFF file. Switches the cab to convolution mode. */
    void loadCabIRFromFile (const juce::File& file)
    {
        if (file.existsAsFile())
        {
            userIRFile = file;
            loadIRInternal (convolution, file);
            usingUserIR = true;
        }
    }

    /** Load the second (blend) IR. */
    void loadCabIRBFromFile (const juce::File& file)
    {
        if (file.existsAsFile())
        {
            userIRFileB = file;
            loadIRInternal (convolutionB, file);
            hasIRB = true;
        }
    }

    /** Revert to the built-in filter cab (clears both IRs). */
    void clearCabIR() noexcept
    {
        usingUserIR = false;
        userIRFile = juce::File();
        hasIRB = false;
        userIRFileB = juce::File();
    }

    /** A<->B blend, 0 = IR A only, 1 = IR B only. */
    void setCabBlend (float blend01) noexcept { cabBlend = juce::jlimit (0.0f, 1.0f, blend01); }

    juce::File getCabIRFile()  const { return userIRFile; }
    juce::File getCabIRFileB() const { return userIRFileB; }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        // --- whammy (pitch shift) in front of the amp, on the mono input ---
        {
            const int n = (int) block.getNumSamples();
            const int numCh = (int) block.getNumChannels();
            if (numCh > 0)
            {
                whammy.process (block.getChannelPointer (0), n);
                for (int ch = 1; ch < numCh; ++ch)
                    juce::FloatVectorOperations::copy (block.getChannelPointer ((size_t) ch),
                                                       block.getChannelPointer (0), n);
            }
        }

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
                const int chs = (int) block.getNumChannels();
                const int n   = (int) block.getNumSamples();

                if (hasIRB && cabBlend > 0.0001f)
                {
                    // Crossfade two IRs: copy dry into scratch for IR B, convolve
                    // each path, then mix A*(1-blend) + B*blend.
                    for (int ch = 0; ch < chs; ++ch)
                        scratch.copyFrom (ch, 0, block.getChannelPointer ((size_t) ch), n);

                    juce::dsp::ProcessContextReplacing<float> ctxA (block);
                    convolution.process (ctxA);

                    juce::dsp::AudioBlock<float> bBlock (scratch);
                    auto bSub = bBlock.getSubBlock (0, (size_t) n);
                    juce::dsp::ProcessContextReplacing<float> ctxB (bSub);
                    convolutionB.process (ctxB);

                    const float bw = cabBlend, aw = 1.0f - cabBlend;
                    for (int ch = 0; ch < chs; ++ch)
                    {
                        auto* a = block.getChannelPointer ((size_t) ch);
                        const auto* b = scratch.getReadPointer (ch);
                        for (int i = 0; i < n; ++i)
                            a[i] = a[i] * aw + b[i] * bw;
                    }
                }
                else
                {
                    juce::dsp::ProcessContextReplacing<float> ctx (block);
                    convolution.process (ctx);
                }
            }
            else
            {
                const int chs = (int) block.getNumChannels();
                const int n   = (int) block.getNumSamples();
                for (int ch = 0; ch < chs; ++ch)
                    cabSim[juce::jmin (ch, 1)].process (block.getChannelPointer ((size_t) ch), n);
            }
        }

        // --- doubler / stereo widener (post-cab; centred mono at width 0) ---
        if (block.getNumChannels() >= 2)
            doubler.process (block.getChannelPointer (0), block.getChannelPointer (1),
                             (int) block.getNumSamples());

        // --- master gain FIRST, so the output stage / limiter operate at the
        //     user's chosen output level, not the hot internal level ---
        juce::dsp::ProcessContextReplacing<float> gainCtx (block);
        outputGain.process (gainCtx);

        // --- output stage: punch + loud (no waveshaping, alias-free), post-fader ---
        {
            const int chs = (int) block.getNumChannels();
            const int n   = (int) block.getNumSamples();
            for (int ch = 0; ch < chs; ++ch)
                outStage[juce::jmin (ch, 1)].process (block.getChannelPointer ((size_t) ch), n);
        }
    }

    int getLatencySamples() const
    {
        return oversampling ? (int) oversampling->getLatencyInSamples() : 0;
    }

private:
    void loadIRInternal (juce::dsp::Convolution& conv, const juce::File& file)
    {
        conv.loadImpulseResponse (file,
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
    bool   hasIRB = false;
    float  cabBlend = 0.0f;
    juce::File userIRFile, userIRFileB;

    AmpCore cores[2];
    CabSim  cabSim[2];
    OutputStage outStage[2];
    Doubler doubler;
    PitchShifter whammy;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::Convolution convolution, convolutionB;
    juce::AudioBuffer<float> scratch;
    juce::dsp::Gain<float> outputGain;
};
} // namespace apex
