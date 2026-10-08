#pragma once

#include "apex/dsp/Filters.h"

#include <array>

namespace apex::dsp
{

/**
 * FizzTamer
 * ---------
 * Dynamic resonance suppression for high-gain guitar, 1.6 .. 9.5 kHz.
 *
 * Sixteen narrow detector bands (two semitones apart) follow the short-term energy of the upper mids
 * and treble. A band is cut only while it stands out from the spectral trend
 * of its neighbours (a resonance, the "fizz" of a cranked amp through a cab),
 * by an amount that follows how far it sticks out, so the overall brightness
 * and the pick attack stay where they were and only the whistling peaks are
 * pulled back. The cuts are dynamic peak filters whose gains are updated at a
 * control rate and smoothed, so nothing zippers.
 *
 * Mono, in place. At amount 0 it is bit-exact bypass.
 */
class FizzTamer
{
public:
    static constexpr int numBands = 16;

    void prepare (double sampleRate);
    void reset() noexcept;

    /** 0..1: how hard resonances are pulled back (up to 12 dB). */
    void setAmount (float amount01) noexcept { amount = amount01 < 0.0f ? 0.0f : (amount01 > 1.0f ? 1.0f : amount01); }

    void process (float* samples, int numSamples) noexcept;

    /** Current cut per band in dB (<= 0), for metering. */
    float getCutDb (int band) const noexcept { return cutDb[(size_t) band]; }

private:
    void updateGains() noexcept;

    double sampleRate = 48000.0;
    float amount = 0.0f;
    bool idle = true;

    std::array<double, numBands> centre {};
    std::array<Biquad, numBands> detectors, cuts;
    std::array<float, numBands> energy {}, cutDb {}, smoothedCut {};
    std::array<double, numBands> cosW {}, alpha {};
    float attack = 0.0f, release = 0.0f, cutSmoothing = 0.0f;
    int controlCounter = 0;
    static constexpr int controlInterval = 16;
};

} // namespace apex::dsp
