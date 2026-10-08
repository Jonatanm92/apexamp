#include "apex/dsp/LowDirt.h"

#include <cmath>

namespace apex::dsp
{

namespace
{
    constexpr double splitHz = 200.0;
    constexpr float drive = 3.2f;    // on the envelope-normalised band
    constexpr float bias  = 0.35f;   // asymmetry -> even harmonics (growl)
    const float biasOffset = std::tanh (bias);
}

void LowDirt::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    split1 = Biquad::lowpass (sampleRate, splitHz);
    split2 = Biquad::lowpass (sampleRate, splitHz);
    postHighpass = Biquad::highpass (sampleRate, 45.0);
    postLowpass  = Biquad::lowpass (sampleRate, 1800.0);
    envelope.setTimes (sampleRate, 0.002, 0.060);
    amountSmoother.setTime (sampleRate, 0.02);
    reset();
}

void LowDirt::reset() noexcept
{
    split1.reset();
    split2.reset();
    postHighpass.reset();
    postLowpass.reset();
    envelope.reset();
    amountSmoother.reset (amount);
    idle = amount <= 0.0f;
}

void LowDirt::process (float* samples, int numSamples) noexcept
{
    if (amount <= 0.0f && amountSmoother.z < 1.0e-4f)
    {
        if (! idle)
        {
            reset();
            idle = true;
        }
        return;
    }
    idle = false;

    for (int i = 0; i < numSamples; ++i)
    {
        const float level = amountSmoother.process (amount);
        const float lows = split2.process (split1.process (samples[i]));
        const float env = envelope.process (lows);

        const float shaped = std::tanh (drive * lows / (env + 1.0e-4f) + bias) - biasOffset;
        const float dirt = postLowpass.process (postHighpass.process (shaped * env));

        samples[i] += 1.1f * level * dirt;
    }
}

} // namespace apex::dsp
