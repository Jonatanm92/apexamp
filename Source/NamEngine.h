#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <cmath>

#include "NAM/get_dsp.h"
#include "BinaryData.h"

/**
 * NamEngine
 * ---------
 * The heart of the NAM-powered ApexAmp. It owns three Neural Amp Modeler rigs
 * (the user's own captures) and three cabinet impulse responses, all embedded
 * in the binary. It can either:
 *   - SINGLE mode: run one rig on its own, or
 *   - BLEND mode : sum all three loudness-matched rigs (layered tone).
 * After the amp stage the signal passes through the selected cabinet IR.
 *
 * All NAM I/O is double precision (NAM_SAMPLE == double); JUCE buffers are
 * float, so we convert at the boundary. NAM rigs were trained at 48 kHz; for
 * milestone 1 we Reset() at the host sample rate (slight voicing shift off
 * 48 k; real resampling is a TODO before release).
 */
class NamEngine
{
public:
    enum class RigMode { Blend = 0, Single = 1 };

    struct Params
    {
        float   inputGainDb  = 0.0f;
        float   outputGainDb = 0.0f;
        RigMode rigMode      = RigMode::Single;
        int     singleIndex  = 0;          // 0..2
        std::array<float, 3> mix { 1.0f, 1.0f, 1.0f }; // blend layer mixes 0..1
        int     irIndex      = 0;          // 0..2
        bool    irEnabled    = true;
        bool    gateEnabled  = false;
        float   gateThreshDb = -60.0f;     // open above this
    };

    NamEngine() = default;

    //==============================================================================
    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        this->sampleRate = sampleRate;
        this->maxBlock   = maxBlockSize;

        loadRigs();

        for (auto& rig : rigs)
            if (rig)
                rig->Reset (sampleRate, maxBlockSize);

        // Scratch buffers (mono, double) for NAM processing.
        monoIn.assign  ((size_t) maxBlockSize, 0.0);
        rigOut.assign  ((size_t) maxBlockSize, 0.0);
        mixBuf.assign  ((size_t) maxBlockSize, 0.0);

        // Cabinet convolution: one convolver per IR so switching is click-free
        // and we never reload on the audio thread.
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = (juce::uint32) maxBlockSize;
        spec.numChannels      = (juce::uint32) juce::jmax (1, numChannels);

        loadConvolvers (spec);

        // Gate state
        gateGain = 1.0f;

        prepared = true;
    }

    void reset()
    {
        for (auto& rig : rigs)
            if (rig)
                rig->Reset (sampleRate, maxBlock);
        for (auto& c : convolvers)
            c.reset();
        gateGain = 1.0f;
    }

    //==============================================================================
    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        if (! prepared)
            return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels  = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0)
            return;

        const int n = juce::jmin (numSamples, maxBlock);

        const double inGain  = juce::Decibels::decibelsToGain ((double) p.inputGainDb);
        const double outGain = juce::Decibels::decibelsToGain ((double) p.outputGainDb);

        // ---- 1) Sum to mono double, apply input gain + optional gate -----------
        const float gateThresh = juce::Decibels::decibelsToGain (p.gateThreshDb);
        const float gateAtk = std::exp (-1.0f / (0.002f * (float) sampleRate)); // 2 ms
        const float gateRel = std::exp (-1.0f / (0.080f * (float) sampleRate)); // 80 ms

        for (int i = 0; i < n; ++i)
        {
            double sum = 0.0;
            for (int ch = 0; ch < numChannels; ++ch)
                sum += (double) buffer.getReadPointer (ch)[i];
            sum /= (double) numChannels;
            sum *= inGain;

            if (p.gateEnabled)
            {
                const float target = (std::abs ((float) sum) > gateThresh) ? 1.0f : 0.0f;
                const float coef   = (target < gateGain) ? gateRel : gateAtk;
                gateGain = target + coef * (gateGain - target);
                sum *= (double) gateGain;
            }

            monoIn[(size_t) i] = sum;
        }

        // ---- 2) Run NAM rig(s) into mixBuf -------------------------------------
        std::fill (mixBuf.begin(), mixBuf.begin() + n, 0.0);

        if (p.rigMode == RigMode::Single)
        {
            const int idx = juce::jlimit (0, 2, p.singleIndex);
            if (rigs[(size_t) idx])
            {
                std::copy (monoIn.begin(), monoIn.begin() + n, rigOut.begin());
                double* in[1]  = { rigOut.data() };
                double* out[1] = { rigOut.data() };
                rigs[(size_t) idx]->process (in, out, n);
                const double g = matchGain[(size_t) idx];
                for (int i = 0; i < n; ++i)
                    mixBuf[(size_t) i] = rigOut[(size_t) i] * g;
            }
        }
        else // Blend: sum all three, loudness-matched, scaled by per-layer mix
        {
            for (size_t r = 0; r < rigs.size(); ++r)
            {
                if (! rigs[r])
                    continue;

                std::copy (monoIn.begin(), monoIn.begin() + n, rigOut.begin());
                double* in[1]  = { rigOut.data() };
                double* out[1] = { rigOut.data() };
                rigs[r]->process (in, out, n);

                const double g = matchGain[r] * (double) juce::jlimit (0.0f, 1.0f, p.mix[r]) / 3.0;
                for (int i = 0; i < n; ++i)
                    mixBuf[(size_t) i] += rigOut[(size_t) i] * g;
            }
        }

        // ---- 3) Write mono result back to all channels (float) -----------------
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                w[i] = (float) mixBuf[(size_t) i];
        }

        // ---- 4) Cabinet IR convolution -----------------------------------------
        if (p.irEnabled)
        {
            const int idx = juce::jlimit (0, 2, p.irIndex);
            juce::dsp::AudioBlock<float>          block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            convolvers[(size_t) idx].process (ctx);
        }

        // ---- 5) Output gain -----------------------------------------------------
        buffer.applyGain ((float) outGain);
    }

    bool isReady() const noexcept { return prepared; }
    double getLoudness (int idx) const noexcept { return loudness[(size_t) juce::jlimit (0, 2, idx)]; }

private:
    //==============================================================================
    void loadRigs()
    {
        struct RigAsset { const char* data; int size; };
        const RigAsset assets[3] = {
            { BinaryData::rig_bite_nam, BinaryData::rig_bite_namSize },
            { BinaryData::rig_body_nam, BinaryData::rig_body_namSize },
            { BinaryData::rig_edge_nam, BinaryData::rig_edge_namSize },
        };

        for (size_t i = 0; i < 3; ++i)
        {
            try
            {
                const std::string text (assets[i].data, (size_t) assets[i].size);
                auto json = nlohmann::json::parse (text);
                rigs[i] = nam::get_dsp (json);

                loudness[i]  = (rigs[i] && rigs[i]->HasLoudness()) ? rigs[i]->GetLoudness() : kTargetLufs;
                matchGain[i] = std::pow (10.0, (kTargetLufs - loudness[i]) / 20.0);
            }
            catch (const std::exception&)
            {
                rigs[i].reset();
                loudness[i]  = kTargetLufs;
                matchGain[i] = 1.0;
            }
        }
    }

    void loadConvolvers (const juce::dsp::ProcessSpec& spec)
    {
        struct IrAsset { const char* data; int size; };
        const IrAsset irs[3] = {
            { BinaryData::ir_ashen_wav,     BinaryData::ir_ashen_wavSize },
            { BinaryData::ir_meshuggah_wav, BinaryData::ir_meshuggah_wavSize },
            { BinaryData::ir_pdi09_wav,     BinaryData::ir_pdi09_wavSize },
        };

        for (size_t i = 0; i < 3; ++i)
        {
            convolvers[i].prepare (spec);
            convolvers[i].loadImpulseResponse (
                irs[i].data, (size_t) irs[i].size,
                juce::dsp::Convolution::Stereo::no,
                juce::dsp::Convolution::Trim::yes,
                0,
                juce::dsp::Convolution::Normalise::yes);
        }
    }

    //==============================================================================
    static constexpr double kTargetLufs = -18.0;

    double sampleRate = 48000.0;
    int    maxBlock   = 512;
    bool   prepared   = false;

    std::array<std::unique_ptr<nam::DSP>, 3> rigs;
    std::array<double, 3> loudness  { kTargetLufs, kTargetLufs, kTargetLufs };
    std::array<double, 3> matchGain { 1.0, 1.0, 1.0 };

    std::array<juce::dsp::Convolution, 3> convolvers;

    std::vector<double> monoIn, rigOut, mixBuf;

    float gateGain = 1.0f;
};
