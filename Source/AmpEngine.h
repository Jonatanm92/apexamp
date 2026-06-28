#pragma once

#include <juce_dsp/juce_dsp.h>
#include "dsp/AmpCore.h"

namespace apex
{
/**
    JUCE-side engine that wraps the pure-C++ AmpCore with the bits that need the
    framework: oversampling around the nonlinear amp, and cabinet IR convolution.

    - The amp (preamp + power amp nonlinearities) runs inside an N-times oversampled
      block so aliasing from the saturation is pushed well above the audible range
      before downsampling.
    - The cab IR convolution is linear, so it runs once at the host rate after
      downsampling (cheaper, no aliasing concern).
    - A synthesized default 4x12-ish cabinet IR is installed at prepare() so the
      plugin makes a usable sound immediately, before the user loads their own WAV.
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
            cores[ch].prepare (osRate);

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                      (juce::uint32) channels };
        convolution.prepare (spec);
        loadDefaultCabIR();

        outputGain.prepare (spec);
        outputGain.setRampDurationSeconds (0.02);

        reset();
    }

    void reset()
    {
        if (oversampling) oversampling->reset();
        for (auto& c : cores) c.reset();
        convolution.reset();
    }

    void setParams (const AmpParams& p)
    {
        for (int ch = 0; ch < 2; ++ch)
            cores[ch].setParams (p);
    }

    void setCabEnabled (bool shouldBeEnabled) noexcept { cabEnabled = shouldBeEnabled; }

    void setMasterGainDb (float db) { outputGain.setGainDecibels (db); }

    /** Load a user IR from a WAV/AIFF file. */
    void loadCabIRFromFile (const juce::File& file)
    {
        if (file.existsAsFile())
            convolution.loadImpulseResponse (file,
                juce::dsp::Convolution::Stereo::yes,
                juce::dsp::Convolution::Trim::yes,
                0,
                juce::dsp::Convolution::Normalise::yes);
    }

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

        // --- linear cab IR convolution at host rate ---
        if (cabEnabled)
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            convolution.process (ctx);
        }

        juce::dsp::ProcessContextReplacing<float> gainCtx (block);
        outputGain.process (gainCtx);
    }

    int getLatencySamples() const
    {
        return oversampling ? (int) oversampling->getLatencyInSamples() : 0;
    }

private:
    /** Build a short synthetic cabinet impulse response so there is sound on day one. */
    void loadDefaultCabIR()
    {
        const int irLen = (int) (hostRate * 0.05); // 50 ms
        juce::AudioBuffer<float> ir (1, irLen);
        ir.clear();
        auto* d = ir.getWritePointer (0);

        // Start from a decaying noise burst...
        juce::Random rng (1234);
        for (int i = 0; i < irLen; ++i)
        {
            const float t = (float) i / (float) irLen;
            const float decay = std::exp (-t * 18.0f);
            d[i] = (rng.nextFloat() * 2.0f - 1.0f) * decay;
        }
        d[0] += 1.0f; // direct spike

        // ...then voice it like a guitar cab: band-limited ~80 Hz - 5 kHz with a
        // presence dip, by running it through a few biquads.
        auto hp  = Biquad::makeHighpass  (hostRate, 85.0,  0.707);
        auto lp  = Biquad::makeLowpass   (hostRate, 5000.0, 0.707);
        auto dip = Biquad::makePeak      (hostRate, 2500.0, 1.2, -6.0);
        auto bump= Biquad::makePeak      (hostRate, 1200.0, 0.8,  3.0);
        for (int i = 0; i < irLen; ++i)
        {
            float s = d[i];
            s = hp.processSample (s);
            s = lp.processSample (s);
            s = dip.processSample (s);
            s = bump.processSample (s);
            d[i] = s;
        }

        convolution.loadImpulseResponse (std::move (ir), hostRate,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::yes);
    }

    static constexpr int oversampleFactorLog2 = 2; // 4x

    double hostRate = 44100.0;
    int    maxBlock = 512;
    int    channels = 2;
    bool   cabEnabled = true;

    AmpCore cores[2];
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::Convolution convolution;
    juce::dsp::Gain<float> outputGain;
};
} // namespace apex
