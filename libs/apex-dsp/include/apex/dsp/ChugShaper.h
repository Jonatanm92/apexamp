#pragma once

#include "apex/dsp/Filters.h"

#include <cstdint>
#include <vector>

namespace apex::dsp
{

/**
 * ChugShaper
 * ----------
 * Adds punch to palm-muted chugs without making sustained chords harsh.
 *
 * A high-gain amp flattens the guitar's dynamics, so a transient detector
 * placed after it barely sees the pick attacks. This one listens to the DI in
 * front of the amp (analyse()), where every pick is a clean, level-independent
 * jump of the short-term envelope over the long-term one, and applies the
 * result after the amp (process()) as a dynamic peak boost around a chosen
 * frequency: low settings give the "thump" of the chug, high settings the
 * pick click. Between attacks the band returns to flat, so held notes and
 * chords are untouched, and the lows below the band are never boosted.
 *
 * Call analyse() and then process() with the same block length every block.
 * At amount 0 process() leaves the signal bit-exact.
 */
class ChugShaper
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /** 0..1, up to +12 dB of dynamic boost. */
    void setAmount (float newAmount) noexcept { amount = newAmount < 0.0f ? 0.0f : (newAmount > 1.0f ? 1.0f : newAmount); }

    /** Centre of the boosted band, 100 Hz .. 4 kHz. */
    void setFrequency (float hz) noexcept;

    /** Samples between the DI given to analyse() and the signal given to
        process() (the amp stage's latency), up to maxDetectorDelay. */
    void setDetectorDelay (int samples) noexcept;
    static constexpr int maxDetectorDelay = 8192;

    void analyse (const float* di, int numSamples) noexcept;
    void process (float* samples, int numSamples) noexcept;

    /** Most recent punch envelope, 0..1 (for a meter / LED). */
    float getPunch() const noexcept { return lastPunch; }

private:
    void updateFilter() noexcept;

    double sampleRate = 48000.0;
    float amount = 0.0f, frequency = 800.0f, designedFrequency = -1.0f;

    Biquad detectorHighpass, band, bandHighpass;
    EnvelopeFollower fast, slow, punch;
    OnePole amountSmoother;

    std::vector<float> punchRing;
    std::int64_t writeIndex = 0, ringMask = 0;
    int detectorDelay = 0;
    float lastPunch = 0.0f;
    bool idle = true;
};

} // namespace apex::dsp
