#pragma once

#include "Biquad.h"
#include <cmath>
#include <algorithm>

namespace apex
{
/**
    Power-amp stage: bias-excursion sag + output-transformer saturation.

    Sag: an envelope follower tracks how hard the section is being driven and
    applies a slow gain reduction, so hard strums "give" and bloom back as they
    decay — the liveliness most budget sims miss. Transformer saturation is a
    slower, rounder nonlinearity than the preamp (more 3rd harmonic, gentle
    compression) plus a high-frequency softening that mimics core losses.
*/
class PowerAmp
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        sagEnv.prepare (fs);
        sagEnv.setTimes (8.0f, 350.0f);  // slow, musical "breathing"
        coreLoss = Biquad::makeLowpass (fs, 9000.0, 0.707);
        reset();
    }

    void reset() noexcept { sagEnv.reset(); coreLoss.reset(); }

    /** sagDepth, drive both 0..1. */
    void setParams (float sagDepthN, float driveN) noexcept
    {
        sagDepth = std::clamp (sagDepthN, 0.0f, 1.0f);
        drive    = 1.0f + driveN * 3.0f;
    }

    inline float processSample (float x) noexcept
    {
        const float env = sagEnv.processSample (x);
        const float gainReduction = 1.0f / (1.0f + sagDepth * env * 2.5f);
        x *= gainReduction;

        x *= drive;
        // Transformer: symmetric-ish soft saturation, rounder than the preamp.
        float y = std::tanh (x * 0.8f) * 1.05f;
        y = coreLoss.processSample (y);
        return y;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    double fs = 44100.0;
    float  sagDepth = 0.3f, drive = 1.0f;
    EnvelopeFollower sagEnv;
    Biquad coreLoss;
};
} // namespace apex
