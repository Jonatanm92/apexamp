#pragma once

#include "Biquad.h"
#include <cmath>
#include <algorithm>

namespace apex
{
/**
    TS-style overdrive boost placed IN FRONT of the amp.

    Metal/rock players almost always run a Tube-Screamer-type pedal into a high-gain
    amp: it high-passes the lows before the distortion (tightness), adds a mid hump,
    and lightly clips — pushing the amp harder and more focused. This models that:

        input high-pass (~720 Hz, tightens) -> mid hump -> soft diode clip
        -> tone low-pass -> level

    Off by default, so it never changes the tone unless the player switches it in.
*/
class Boost
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        hp  = Biquad::makeHighpass (fs, 720.0, 0.707);
        mid = Biquad::makePeak     (fs, 720.0, 0.7, 4.0);
        recalcTone();
        reset();
    }

    void reset() noexcept { hp.reset(); mid.reset(); post.reset(); }

    void setParams (bool on, float drive01, float tone01) noexcept
    {
        enabled = on;
        drive   = 1.0f + std::clamp (drive01, 0.0f, 1.0f) * 24.0f;
        toneHz  = 2000.0f + std::clamp (tone01, 0.0f, 1.0f) * 5000.0f;
        recalcTone();
    }

    inline float processSample (float x) noexcept
    {
        if (! enabled)
            return x;

        float h = hp.processSample (x);
        h = mid.processSample (h);
        float d = h * drive + 0.04f;            // slight asymmetry => even harmonics
        float c = d / (1.0f + std::fabs (d));   // diode-style soft clip
        c = post.processSample (c);
        return c * 0.9f;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    void recalcTone() noexcept { post = Biquad::makeLowpass (fs, toneHz, 0.707); }

    double fs = 44100.0;
    bool   enabled = false;
    float  drive = 6.0f, toneHz = 4500.0f;
    Biquad hp, mid, post;
};
} // namespace apex
