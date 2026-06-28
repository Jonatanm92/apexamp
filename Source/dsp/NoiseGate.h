#pragma once

#include "Biquad.h"
#include <cmath>
#include <algorithm>

namespace apex
{
/**
    Input noise gate / downward gate for high-gain clarity.

    High-gain amps amplify everything, including the noise floor (hum, hiss,
    single-coil buzz) in the gaps between notes and under palm mutes. Gating the
    DI *before* the preamp stops that noise from ever being amplified, which is
    what gives tight, clean chugs and silence between riffs.

    Classic attack / hold / release envelope gate:
      - a fast detector tracks the input level
      - above threshold: open fast (attack)
      - below threshold: stay open for `hold` ms, then close (release)
    The smoothed gain multiplies the signal so it opens/closes click-free.
*/
class NoiseGate
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        detector.prepare (fs);
        detector.setTimes (0.2f, 5.0f);   // fast level detection
        attackMs = 1.0f; holdMs = 40.0f; releaseMs = 120.0f;
        updateCoeffs();
        setThreshold (-60.0f);
        reset();
    }

    void reset() noexcept
    {
        detector.reset();
        gain = 0.0f;
        holdCounter = 0;
    }

    /** Threshold in dBFS (of the DI). <= -79 dB disables the gate (always open). */
    void setThreshold (float thresholdDb) noexcept
    {
        threshLin = std::pow (10.0f, thresholdDb / 20.0f);
        enabled   = thresholdDb > -79.0f;
    }

    inline float processSample (float x) noexcept
    {
        if (! enabled)
            return x;

        const float level = detector.processSample (x);

        float targetGain;
        if (level >= threshLin)
        {
            targetGain = 1.0f;
            holdCounter = holdSamples;
        }
        else if (holdCounter > 0)
        {
            --holdCounter;
            targetGain = 1.0f;
        }
        else
        {
            targetGain = 0.0f;
        }

        const float c = (targetGain > gain) ? atkCoeff : relCoeff;
        gain += c * (targetGain - gain);
        return x * gain;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    void updateCoeffs() noexcept
    {
        atkCoeff = 1.0f - std::exp (-1.0f / (float (fs) * attackMs  * 0.001f));
        relCoeff = 1.0f - std::exp (-1.0f / (float (fs) * releaseMs * 0.001f));
        holdSamples = (int) (fs * holdMs * 0.001);
    }

    double fs = 44100.0;
    float  attackMs = 1.0f, holdMs = 40.0f, releaseMs = 120.0f;
    float  atkCoeff = 0.0f, relCoeff = 0.0f;
    int    holdSamples = 0, holdCounter = 0;

    float  threshLin = 0.001f;
    bool   enabled = true;
    float  gain = 0.0f;

    EnvelopeFollower detector;
};
} // namespace apex
