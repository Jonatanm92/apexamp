#include "apex/dsp/ChugShaper.h"

#include <algorithm>
#include <cmath>

namespace apex::dsp
{

namespace
{
    constexpr float maxBoostDb = 12.0f;
    constexpr float bandQ = 0.8f;

    // Envelope ratio (short / long term) that counts as a pick attack, and the
    // ratio at which the punch is at its full depth.
    constexpr float attackStart = 1.25f;   // ~ +2 dB
    constexpr float attackFull  = 3.2f;    // ~ +10 dB

    // Below this the DI is noise or a ringing tail, never an attack.
    constexpr float noiseFloor = 5.0e-4f;  // ~ -66 dBFS
}

void ChugShaper::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    detectorHighpass = Biquad::highpass (sampleRate, 60.0);
    fast.setTimes (sampleRate, 0.0002, 0.006);
    slow.setTimes (sampleRate, 0.020, 0.150);
    punch.setTimes (sampleRate, 0.0005, 0.045);
    amountSmoother.setTime (sampleRate, 0.02);

    std::int64_t size = 1;
    while (size < (std::int64_t) maxBlockSize + maxDetectorDelay + 1)
        size <<= 1;
    punchRing.assign ((size_t) size, 0.0f);
    ringMask = size - 1;

    designedFrequency = -1.0f;
    updateFilter();
    reset();
}

void ChugShaper::reset() noexcept
{
    detectorHighpass.reset();
    band.reset();
    bandHighpass.reset();
    fast.reset();
    slow.reset();
    punch.reset();
    amountSmoother.reset (amount);
    std::fill (punchRing.begin(), punchRing.end(), 0.0f);
    writeIndex = 0;
    lastPunch = 0.0f;
    idle = amount <= 0.0f;
}

void ChugShaper::setFrequency (float hz) noexcept
{
    frequency = std::clamp (hz, 100.0f, 4000.0f);
}

void ChugShaper::setDetectorDelay (int samples) noexcept
{
    detectorDelay = std::clamp (samples, 0, maxDetectorDelay);
}

void ChugShaper::updateFilter() noexcept
{
    if (std::abs (frequency - designedFrequency) <= 0.002f * frequency)
        return;
    designedFrequency = frequency;
    band.copyCoefficients (Biquad::bandpass (sampleRate, frequency, bandQ));
    bandHighpass.copyCoefficients (Biquad::highpass (sampleRate, 0.4 * frequency));
}

void ChugShaper::analyse (const float* di, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float x = detectorHighpass.process (di[i]);
        const float f = fast.process (x);
        const float s = slow.process (x);

        float attack = 0.0f;
        if (f > noiseFloor)
            attack = std::clamp ((f / (s + 1.0e-9f) - attackStart) / (attackFull - attackStart), 0.0f, 1.0f);

        punchRing[(size_t) (writeIndex & ringMask)] = punch.process (attack);
        ++writeIndex;
    }
    lastPunch = punch.z;
}

void ChugShaper::process (float* samples, int numSamples) noexcept
{
    if (amount <= 0.0f && amountSmoother.z < 1.0e-4f)
    {
        // Exact bypass. Start the band filter from rest next time.
        if (! idle)
        {
            band.reset();
            bandHighpass.reset();
            amountSmoother.reset (0.0f);
            idle = true;
        }
        return;
    }
    idle = false;
    updateFilter();

    std::int64_t read = writeIndex - numSamples - detectorDelay;
    for (int i = 0; i < numSamples; ++i, ++read)
    {
        const float depth = amountSmoother.process (amount);
        const float p = read >= 0 ? punchRing[(size_t) (read & ringMask)] : 0.0f;

        // Peak boost of (maxBoostDb * depth * p) dB at the band centre:
        // x + (g - 1) * bandpass(x), with g in linear gain.
        const float g = std::exp (0.11512925f * maxBoostDb * depth * p) - 1.0f;   // ln(10)/20
        // A steeper skirt below the band keeps the boost off the low end.
        const float b = bandHighpass.process (band.process (samples[i]));
        samples[i] += g * b;
    }
}

} // namespace apex::dsp
