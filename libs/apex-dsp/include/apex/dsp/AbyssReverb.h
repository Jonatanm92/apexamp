#pragma once

#include "apex/dsp/Filters.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace apex::dsp
{

/**
 * AbyssReverb
 * -----------
 * Eight-line feedback delay network (Householder mixing, per-line absorption
 * set from the decay time, slow modulation on every line) behind a pre-delay
 * and four allpass diffusers per side. Low end below ~150 Hz is kept out of the
 * tank, so a down-tuned rhythm part stays tight under a long tail.
 *
 * ABYSS: an octave-DOWN shimmer. Part of the tail is pitched down an octave
 * (two crossfaded grains, which a dense tail masks completely) and fed back
 * into the tank, so every pass sinks
 * another octave until it falls below the band and dies away: chords bloom
 * into a dark, descending wash instead of the usual octave-up sparkle. The
 * feedback is normalised to the tank's energy gain, so it is stable at any
 * decay setting.
 *
 * The dry signal passes through untouched; the tail is added on top. When
 * switched off the input stops feeding the tank and the tail rings out
 * (spillover). Output is identical for any host block size.
 */
class AbyssReverb
{
public:
    static constexpr int numLines = 8;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setEnabled (bool shouldBeOn) noexcept { enabled = shouldBeOn; }
    void setDecay (float seconds) noexcept;       // RT60, 0.3 .. 30 s
    void setAbyss (float amount01) noexcept;      // octave-down shimmer
    void setTone (float tone01) noexcept;         // dark .. bright tail
    void setMix (float level01) noexcept   { mix = level01; }

    /** Adds the tail to the buffers. right may be null (mono). */
    void process (float* left, float* right, int numSamples) noexcept;

    bool isRinging() const noexcept { return ! asleep; }

private:
    struct Allpass
    {
        std::vector<float> buffer;
        int index = 0;
        float gain = 0.62f;

        void prepare (int length) { buffer.assign ((size_t) std::max (1, length), 0.0f); index = 0; }
        void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); index = 0; }
        float process (float x) noexcept
        {
            const float delayed = buffer[(size_t) index];
            const float v = x + gain * delayed;
            buffer[(size_t) index] = v;
            if (++index >= (int) buffer.size()) index = 0;
            return delayed - gain * v;
        }
    };

    /** Octave down: two read heads drifting at half speed through a short
        buffer, each faded in and out with a power-complementary window. */
    struct OctaveDown
    {
        std::vector<float> buffer;
        std::int64_t writeIndex = 0, mask = 0;
        double phase = 0.0, phaseStep = 0.0, span = 0.0, minimum = 0.0;

        void prepare (double sampleRate);
        void reset() noexcept;
        float process (float x) noexcept;
        float read (double delay) const noexcept;
    };

    void updateDecay() noexcept;
    void updateTone() noexcept;
    float readLine (int line, double delaySamples) const noexcept;

    double sampleRate = 48000.0;
    bool enabled = false, asleep = true;
    float decay = 3.0f, designedDecay = -1.0f, abyss = 0.0f, mix = 0.25f;
    float tone = 0.5f, designedTone = -1.0f;

    // tank
    std::array<std::vector<float>, numLines> lines;
    std::array<double, numLines> lineLength {};
    std::array<float, numLines> lineGain {};
    std::array<OnePole, numLines> absorb;
    std::array<SlowOscillator, numLines> modulators;
    std::int64_t writeIndex = 0, lineMask = 0;
    float modulationDepth = 0.0f;
    float wetScale = 1.0f, shimmerScale = 0.0f;

    // input
    std::vector<float> preDelay[2];
    int preDelayIndex = 0;
    Biquad inputHighpass[2], inputLowpass[2], outputHighpass[2];
    Allpass diffusers[2][4];
    OnePole send, mixSmoother, abyssSmoother;

    // abyss
    OctaveDown octaveDown;
    Biquad shimmerHighpass, shimmerLowpass;
    float shimmer = 0.0f;

    int quietSamples = 0, sleepAfter = 0;
};

} // namespace apex::dsp
