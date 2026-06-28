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
        hpf      = Biquad::makeHighpass  (fs, 60.0,   0.707); // keep the power stage tight
        presence = Biquad::makeHighShelf (fs, 3000.0, 0.7, 2.5); // grind before the transformer
        coreLoss = Biquad::makeLowpass   (fs, 12000.0, 0.707);   // gentle, not dark
        reset();
    }

    void reset() noexcept { sagEnv.reset(); hpf.reset(); presence.reset(); coreLoss.reset(); }

    /** sagDepth, drive both 0..1. */
    void setParams (float sagDepthN, float driveN) noexcept
    {
        sagDepth = std::clamp (sagDepthN, 0.0f, 1.0f);
        drive    = 1.0f + driveN * 3.0f;
    }

    inline float processSample (float x) noexcept
    {
        x = hpf.processSample (x);

        const float env = sagEnv.processSample (x);
        const float gainReduction = 1.0f / (1.0f + sagDepth * env * 2.5f);
        x *= gainReduction;

        x *= drive;
        x = presence.processSample (x);
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
    Biquad hpf;
    Biquad presence;
    Biquad coreLoss;
};
} // namespace apex
