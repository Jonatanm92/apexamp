#pragma once

#include "apex/dsp/Filters.h"

namespace apex::dsp
{

/**
 * LowDirt
 * -------
 * Parallel growl for the low end. The band below ~200 Hz is split off,
 * normalised by its own envelope and pushed through an asymmetric fuzz, so the
 * character is the same whether you dig in or play softly, then put back at the
 * band's original level and low-passed before it is mixed in. Down-tuned riffs
 * get grind and note definition without the full-range signal getting any
 * fizzier, and silence stays silent (the layer follows the band's envelope).
 *
 * Sits after the amp, before the cab. At amount 0 it is bit-exact bypass.
 */
class LowDirt
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    /** 0..1: level of the dirt layer (1 = as loud as the band it came from). */
    void setAmount (float newAmount) noexcept { amount = newAmount < 0.0f ? 0.0f : (newAmount > 1.0f ? 1.0f : newAmount); }

    void process (float* samples, int numSamples) noexcept;

private:
    double sampleRate = 48000.0;
    float amount = 0.0f;
    bool idle = true;

    Biquad split1, split2, postHighpass, postLowpass;
    EnvelopeFollower envelope;
    OnePole amountSmoother;
};

} // namespace apex::dsp
