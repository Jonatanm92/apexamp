#pragma once

#include "Biquad.h"
#include <cmath>
#include <algorithm>

namespace apex
{
/**
    Transient-aware "chug" clarity enhancer (our answer to Thall's Chug control).

    How it beats a static EQ boost: it splits the signal at ~200 Hz with a
    Linkwitz-Riley 4th-order crossover so the sub-bass / palm-mute weight passes
    through completely untouched. The high band gets a dynamic upper-mid boost
    (800 Hz - 3 kHz) whose amount is driven by a transient detector (fast vs slow
    envelope ratio). The boost rises on pick attacks and palm mutes for definition,
    then backs off during sustain so chords don't get harsh. Sub weight stays;
    attack clarity is added only when you dig in.
*/
class ChugEnhancer
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        buildCrossover();

        // Band-pass extractor for the upper-mid content we emphasise on attacks.
        midExtractA = Biquad::makeHighpass (fs, 800.0, 0.707);
        midExtractB = Biquad::makeLowpass  (fs, 3000.0, 0.707);

        fastEnv.prepare (fs); fastEnv.setTimes (0.5f, 20.0f);
        slowEnv.prepare (fs); slowEnv.setTimes (15.0f, 200.0f);
        reset();
    }

    void reset() noexcept
    {
        lpA.reset(); lpB.reset(); hpA.reset(); hpB.reset();
        midExtractA.reset(); midExtractB.reset();
        fastEnv.reset(); slowEnv.reset();
        smoothedBoost = 0.0f;
    }

    /** amount: 0..1 chug intensity. */
    void setAmount (float a) noexcept { amount = std::clamp (a, 0.0f, 1.0f); }

    inline float processSample (float x) noexcept
    {
        // Linkwitz-Riley split (two cascaded Butterworth sections each band).
        const float lowBand  = lpB.processSample (lpA.processSample (x));
        const float highBand = hpB.processSample (hpA.processSample (x));

        // Transient detection on the full signal.
        const float fast = fastEnv.processSample (x);
        const float slow = slowEnv.processSample (x) + 1.0e-6f;
        float transient = (fast - slow) / slow;          // >0 during attacks
        transient = std::clamp (transient, 0.0f, 1.0f);

        // Smooth so the boost glides rather than zipper-noises.
        const float target = transient * amount;
        smoothedBoost += 0.01f * (target - smoothedBoost);

        // Extract upper-mid content and add it back to the high band, scaled by
        // the (smoothed) transient amount. Sub-bass low band is never touched.
        const float midContent = midExtractB.processSample (midExtractA.processSample (highBand));
        const float emphasis = smoothedBoost * 1.8f;     // up to ~+5 dB of mids on hits
        const float high = highBand + midContent * emphasis;

        return lowBand + high;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    void buildCrossover() noexcept
    {
        const double f = 200.0;
        lpA = Biquad::makeLowpass  (fs, f, 0.7071);
        lpB = Biquad::makeLowpass  (fs, f, 0.7071);
        hpA = Biquad::makeHighpass (fs, f, 0.7071);
        hpB = Biquad::makeHighpass (fs, f, 0.7071);
    }

    double fs = 44100.0;
    float  amount = 0.0f;
    float  smoothedBoost = 0.0f;

    Biquad lpA, lpB, hpA, hpB;
    Biquad midExtractA, midExtractB;
    EnvelopeFollower fastEnv, slowEnv;
};

/**
    Low Dirt — a parallel low-band growl layer (inspired by Thall's "Low Dirt").
    It isolates a low band, saturates it gently, and mixes it back so down-tuned
    riffs get growl/bite without muddying the full-range signal.
*/
class LowDirt
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        band = Biquad::makeLowpass (fs, 220.0, 0.707);
        postHp = Biquad::makeHighpass (fs, 60.0, 0.707); // keep rumble out
        reset();
    }

    void reset() noexcept { band.reset(); postHp.reset(); }

    void setParams (float driveAmount, float mixAmount) noexcept
    {
        drive = 1.0f + driveAmount * 8.0f;
        mix   = std::clamp (mixAmount, 0.0f, 1.0f);
    }

    inline float processSample (float x) noexcept
    {
        float b = band.processSample (x);
        b = std::tanh (b * drive);          // gentle growl
        b = postHp.processSample (b);
        return x + b * mix;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    double fs = 44100.0;
    float  drive = 1.0f, mix = 0.0f;
    Biquad band, postHp;
};
} // namespace apex
