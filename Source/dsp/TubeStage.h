#pragma once

#include "Biquad.h"
#include <cmath>

namespace apex
{
/**
    A single 12AX7-style triode gain stage.

    Per sample:  DC block -> pre-clip high-pass (tighten) -> input gain
                 -> asymmetric saturation -> bias shift -> presence lift
                 -> Miller-capacitance lowpass.

    Two things make this sound like an amp instead of a fuzz pedal:

    1. The *pre-clip high-pass* removes low end BEFORE the nonlinearity. Without
       it, bass intermodulates through the saturation and turns to mud. Cascading
       progressively higher corners across stages is exactly how real high-gain
       amps stay tight.

    2. The asymmetric shaper: the positive half is clipped harder (tanh, models
       grid conduction) while the negative half compresses more gently (scaled
       atan, plate saturation). The blend makes even + odd harmonics for a richer,
       grainier texture than a symmetric clipper.
*/
class TubeStage
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        dcBlock.setCutoff (fs, 12.0);
        setHighpass (100.0);
        // Post-clip upper-mid lift = the "grind"/presence a cathode-bypass cap adds.
        presence = Biquad::makeHighShelf (fs, 1800.0, 0.7, 2.5);
        // Interstage lowpass — kept high so the cab does the final taming.
        miller = Biquad::makeLowpass (fs, 15000.0, 0.707);
        reset();
    }

    void reset() noexcept
    {
        dcBlock.reset();
        preHP.reset();
        presence.reset();
        miller.reset();
    }

    /** Pre-clip high-pass corner (Hz). Higher = tighter / less bass into the clip. */
    void setHighpass (double freq) noexcept
    {
        preHP = Biquad::makeHighpass (fs, freq, 0.707);
    }

    /** drive: linear input gain. bias: -0.1..+0.1 tilts harmonic content. */
    void setParams (float driveLinear, float biasAmount) noexcept
    {
        drive = driveLinear;
        bias  = biasAmount;
    }

    inline float processSample (float x) noexcept
    {
        x = dcBlock.processSample (x);
        x = preHP.processSample (x);   // tighten lows BEFORE clipping
        x *= drive;

        // Asymmetric waveshaper around a shifted operating point.
        const float shifted = x + bias;
        float y;
        if (shifted >= 0.0f)
            y = std::tanh (shifted);                 // grid side: harder knee
        else
            y = 0.7f * std::atan (1.3f * shifted);   // plate side: softer, asymmetric

        y -= bias * 0.8f; // re-centre so DC offset stays small before the blocker

        y = presence.processSample (y);
        y = miller.processSample (y);
        return y;
    }

    /** Process an interleaved-free mono buffer in place. */
    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    double fs = 44100.0;
    float  drive = 1.0f;
    float  bias  = 0.0f;

    DCBlocker dcBlock;
    Biquad    preHP;
    Biquad    presence;
    Biquad    miller;
};
} // namespace apex
