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
        presence.reset(); edge.reset(); lp1.reset(); lp2.reset();
    }

    inline float processSample (float x) noexcept
    {
        x = hp.processSample (x);
        x = thump.processSample (x);
        x = scoop.processSample (x);
        x = presence.processSample (x);
        x = edge.processSample (x);
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
        hp       = Biquad::makeHighpass  (fs, 85.0,   0.8);
        thump    = Biquad::makePeak      (fs, 100.0,  1.0,  1.0);  // a little chunk, not boom
        scoop    = Biquad::makePeak      (fs, 450.0,  1.0, -2.5);  // clear the boxy mud
        presence = Biquad::makePeak      (fs, 3500.0, 1.2,  5.0);  // bite / cut
        edge     = Biquad::makeHighShelf (fs, 4500.0, 0.7,  3.0);  // the "gnarl" zone
        lp1      = Biquad::makeLowpass   (fs, 6500.0, 0.707);      // speaker roll-off,
        lp2      = Biquad::makeLowpass   (fs, 6800.0, 0.707);      // but high enough to keep grain
    }

    double fs = 44100.0;
    Biquad hp, thump, scoop, presence, edge, lp1, lp2;
};
} // namespace apex
