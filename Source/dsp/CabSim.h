#pragma once

#include "Biquad.h"

namespace apex
{
/**
    Filter-based guitar speaker / cabinet emulation (default, no IR required).

    A real impulse response is the gold standard, but a *synthesized noise* IR
    sounds phasey and harsh. This deterministic EQ voicing instead models the
    gross frequency response of a closed-back 4x12 with a V30-style speaker:

      - steep high-pass ~80 Hz       (removes sub-bass flub)
      - low-mid thump  ~110 Hz       (chunk / palm-mute weight)
      - boxiness scoop ~450 Hz       (clears mud)
      - presence bump  ~2.8 kHz      (pick attack / cut)
      - steep low-pass ~5 kHz (x2)   (the speaker roll-off that tames digital
                                       fizz — the single biggest "produced" upgrade)

    It is smooth, predictable, and CPU-cheap. Users who want a specific cab can
    still load their own IR, which bypasses this voicing in favour of convolution.
*/
class CabSim
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        build();
        reset();
    }

    void reset() noexcept
    {
        hp.reset(); thump.reset(); scoop.reset();
        presence.reset(); lp1.reset(); lp2.reset();
    }

    inline float processSample (float x) noexcept
    {
        x = hp.processSample (x);
        x = thump.processSample (x);
        x = scoop.processSample (x);
        x = presence.processSample (x);
        x = lp1.processSample (x);
        x = lp2.processSample (x);
        return x;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    void build() noexcept
    {
        hp       = Biquad::makeHighpass (fs, 80.0,   0.707);
        thump    = Biquad::makePeak     (fs, 110.0,  1.1,  3.5);
        scoop    = Biquad::makePeak     (fs, 450.0,  1.0, -3.0);
        presence = Biquad::makePeak     (fs, 2800.0, 1.3,  4.0);
        lp1      = Biquad::makeLowpass  (fs, 5000.0, 0.707);
        lp2      = Biquad::makeLowpass  (fs, 5200.0, 0.707); // cascade => steeper roll-off
    }

    double fs = 44100.0;
    Biquad hp, thump, scoop, presence, lp1, lp2;
};
} // namespace apex
