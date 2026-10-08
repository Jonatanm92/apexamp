#include "apex/dsp/FizzTamer.h"

#include <algorithm>
#include <cmath>

namespace apex::dsp
{

namespace
{
    constexpr double lowestHz = 1600.0, highestHz = 9500.0;
    constexpr double detectorQ = 6.0, cutQ = 5.0;
    constexpr float thresholdDb = 2.0f;    // how far a band may stand out before it is cut
    constexpr float ratio = 1.0f;          // dB of cut per dB above the threshold, at full amount
    constexpr float maxCutDb = 12.0f;
}

void FizzTamer::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    const double top = std::min (highestHz, 0.42 * sampleRate);
    for (int b = 0; b < numBands; ++b)
    {
        centre[(size_t) b] = lowestHz * std::pow (top / lowestHz, (double) b / (double) (numBands - 1));
        detectors[(size_t) b] = Biquad::bandpass (sampleRate, centre[(size_t) b], detectorQ);
        const double w = 2.0 * kPi * centre[(size_t) b] / sampleRate;
        cosW[(size_t) b] = std::cos (w);
        alpha[(size_t) b] = std::sin (w) / (2.0 * cutQ);
    }
    attack  = (float) (1.0 - std::exp (-1.0 / (0.002 * sampleRate)));
    release = (float) (1.0 - std::exp (-1.0 / (0.050 * sampleRate)));
    cutSmoothing = (float) (1.0 - std::exp (-(double) controlInterval / (0.008 * sampleRate)));
    reset();
}

void FizzTamer::reset() noexcept
{
    for (auto& d : detectors) d.reset();
    for (auto& c : cuts) { c = Biquad(); }
    energy.fill (0.0f);
    cutDb.fill (0.0f);
    smoothedCut.fill (0.0f);
    controlCounter = 0;
    idle = amount <= 0.0f;
}

void FizzTamer::updateGains() noexcept
{
    // Band levels in dB, and the trend they should follow: a straight line
    // through the levels (least squares over the log-spaced bands), fitted a
    // second time without the bands that stick out, so the resonances do not
    // pull the trend up. A dark or bright tone as a whole is left alone.
    std::array<float, numBands> level {};
    for (int b = 0; b < numBands; ++b)
        level[(size_t) b] = 10.0f * std::log10 (energy[(size_t) b] + 1.0e-12f);

    float meanX = 0.0f, meanY = 0.0f, slope = 0.0f;
    std::array<bool, numBands> use {};
    use.fill (true);
    for (int pass = 0; pass < 2; ++pass)
    {
        float n = 0.0f;
        meanX = meanY = 0.0f;
        for (int b = 0; b < numBands; ++b)
            if (use[(size_t) b]) { meanX += (float) b; meanY += level[(size_t) b]; n += 1.0f; }
        meanX /= n;
        meanY /= n;
        float sxy = 0.0f, sxx = 0.0f;
        for (int b = 0; b < numBands; ++b)
            if (use[(size_t) b])
            {
                sxy += ((float) b - meanX) * (level[(size_t) b] - meanY);
                sxx += ((float) b - meanX) * ((float) b - meanX);
            }
        slope = sxx > 0.0f ? sxy / sxx : 0.0f;
        for (int b = 0; b < numBands; ++b)
            use[(size_t) b] = level[(size_t) b] - (meanY + slope * ((float) b - meanX)) < thresholdDb;
    }
    const bool audible = meanY > -75.0f;

    for (int b = 0; b < numBands; ++b)
    {
        const float trend = meanY + slope * ((float) b - meanX);
        const float excess = level[(size_t) b] - trend - thresholdDb;
        const float target = audible ? -std::min (maxCutDb, amount * ratio * std::max (0.0f, excess)) : 0.0f;
        smoothedCut[(size_t) b] += cutSmoothing * (target - smoothedCut[(size_t) b]);
        cutDb[(size_t) b] = smoothedCut[(size_t) b];

        // RBJ peaking filter with the new gain (the frequency never changes)
        const double A = std::pow (10.0, (double) smoothedCut[(size_t) b] / 40.0);
        const double al = alpha[(size_t) b], c = cosW[(size_t) b];
        const double a0 = 1.0 + al / A;
        auto& f = cuts[(size_t) b];
        f.b0 = (float) ((1.0 + al * A) / a0);
        f.b1 = (float) (-2.0 * c / a0);
        f.b2 = (float) ((1.0 - al * A) / a0);
        f.a1 = (float) (-2.0 * c / a0);
        f.a2 = (float) ((1.0 - al / A) / a0);
    }
}

void FizzTamer::process (float* samples, int numSamples) noexcept
{
    if (amount <= 0.0f)
    {
        bool resting = true;
        for (float c : smoothedCut)
            resting = resting && c > -0.01f;
        if (resting)
        {
            if (! idle)
                reset();
            idle = true;
            return;
        }
    }
    idle = false;

    for (int i = 0; i < numSamples; ++i)
    {
        const float x = samples[i];
        for (int b = 0; b < numBands; ++b)
        {
            const float d = detectors[(size_t) b].process (x);
            const float e = d * d;
            energy[(size_t) b] += (e > energy[(size_t) b] ? attack : release) * (e - energy[(size_t) b]);
        }
        if (++controlCounter >= controlInterval)
        {
            controlCounter = 0;
            updateGains();
        }
        float y = x;
        for (auto& f : cuts)
            y = f.process (y);
        samples[i] = y;
    }
}

} // namespace apex::dsp
