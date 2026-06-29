#pragma once

#include "Biquad.h"
#include "TubeStage.h"
#include <cmath>
#include <algorithm>

namespace apex
{
enum class PreampChannel
{
    tight = 0,  // focused, present rhythm voicing (Graphene "Blue"-style)
    scoop       // scooped, aggressive lead/chug voicing (Graphene "Purple"-style)
};

/**
    Dual-voiced cascaded-triode preamp.

    Controls:
      gain      0..1  overall preamp drive (quadratic taper inside).
      push      0..1  extra saturation / compression on top of the gain stages.
      tight     0..1  pre-saturation high-pass — removes flub before clipping so
                      palm mutes stay defined under high gain.
      superCut  0..1  scoop channel only: blends in a mid-scooped band so the
                      scoop is baked into the distortion, not just EQ'd after it.
      bias      tube asymmetry shared by all stages.

    Improvement over Graphene: the scoop is applied *before* the tube stages, so
    the saturation reacts to the already-scooped spectrum — you get a genuinely
    different distortion texture per channel rather than identical clipping with a
    different EQ stuck on the end.
*/
class DualChannelPreamp
{
public:
    static constexpr int maxStages = 4;

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        for (auto& s : stages) s.prepare (fs);

        tightHP  = Biquad::makeHighpass (fs, 80.0, 0.707);
        scoopPre = Biquad::makePeak     (fs, 550.0, 1.0, 0.0); // set in recalc
        // Bright pre-emphasis drives the upper harmonics that become "grind".
        brightCap = Biquad::makeHighShelf (fs, 2200.0, 0.7, 5.0);
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : stages) s.reset();
        tightHP.reset(); scoopPre.reset(); brightCap.reset();
    }

    void setChannel (PreampChannel c) noexcept { channel = c; recalc(); }

    void setParams (float gainN, float pushN, float tightN,
                    float superCutN, float biasN) noexcept
    {
        gain     = gainN;
        push     = std::clamp (pushN, 0.0f, 1.0f);
        tight    = std::clamp (tightN, 0.0f, 1.0f);
        superCut = std::clamp (superCutN, 0.0f, 1.0f);
        bias     = biasN;
        recalc();
    }

    /**
        Pitch-adaptive tightness (unique feature). `amount` 0..1 blends in a
        low-cut that tracks the detected fundamental `hz`, so the low end stays
        equally tight whether you're in E standard or dropped to A/G — the HPF
        follows your tuning instead of being fixed. Heavily smoothed so it tracks
        the riff's register, and capped so single notes / leads keep their body.
    */
    void setAdaptiveTight (float amount, float hz) noexcept
    {
        autoTight = std::clamp (amount, 0.0f, 1.0f);
        if (hz > 40.0f && hz < 500.0f)
            smoothedHz += 0.05f * (hz - smoothedHz);
        recalc();
    }

    inline float processSample (float x) noexcept
    {
        // Pre-clip high-pass: more "tight" => higher corner already baked into coeffs.
        x = tightHP.processSample (x);

        if (channel == PreampChannel::scoop)
        {
            // Bake the scoop in before saturation.
            const float scooped = scoopPre.processSample (x);
            x = x * (1.0f - superCut) + scooped * superCut;
        }

        // Both channels get a bright lift into the stages so there is upper-harmonic
        // content for the saturation to turn into grain/bite.
        x = brightCap.processSample (x);

        for (int i = 0; i < numActiveStages; ++i)
            x = stages[i].processSample (x);

        // Push: an extra soft-clip that adds compression/saturation on demand.
        if (push > 0.0f)
        {
            const float d = 1.0f + push * 4.0f;
            const float clipped = std::tanh (x * d) / std::tanh (d);
            x = x * (1.0f - push) + clipped * push;
        }

        return x;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    void recalc() noexcept
    {
        // Channel sets stage count and base tightness.
        float baseHP;
        if (channel == PreampChannel::tight)
        {
            numActiveStages = 3;
            baseHP = 90.0f + tight * 90.0f;   // 90..180 Hz
        }
        else
        {
            numActiveStages = 4;                            // more gain stages
            baseHP = 80.0f + tight * 80.0f;   // 80..160 Hz
            scoopPre = Biquad::makePeak (fs, 550.0, 1.1, -10.0f); // scoop shape
        }

        // Adaptive tightness: blend the base corner toward one that tracks the
        // detected fundamental (just below it), so tightness scales with tuning.
        if (autoTight > 0.0f)
        {
            const float adaptive = std::clamp (smoothedHz * 0.9f, 45.0f, 140.0f);
            baseHP = baseHP * (1.0f - autoTight) + adaptive * autoTight;
        }

        tightHP = Biquad::makeHighpass (fs, baseHP, 0.707);

        // Quadratic gain taper => usable lower half, big top half.
        const float g = gain * gain;
        const float driveLin = 1.0f + g * 22.0f;

        // Progressively tighten the low end deeper into the chain. Each stage
        // high-passes a little higher than the last, so bass can't stack up and
        // turn to mud through the cascade — this is what keeps high gain "chuggy"
        // instead of "woofy".
        for (int i = 0; i < maxStages; ++i)
        {
            stages[i].setParams (driveLin, bias);
            stages[i].setHighpass (baseHP + (float) i * 40.0f);
        }
    }

    double fs = 44100.0;
    PreampChannel channel = PreampChannel::tight;
    int numActiveStages = 3;

    float gain = 0.5f, push = 0.0f, tight = 0.3f, superCut = 0.0f, bias = 0.02f;
    float autoTight = 0.0f, smoothedHz = 82.0f;

    TubeStage stages[maxStages];
    Biquad tightHP, scoopPre, brightCap;
};
} // namespace apex
