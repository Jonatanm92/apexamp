#pragma once

#include "Biquad.h"
#include <cmath>

namespace apex
{
/**
    A single 12AX7-style triode gain stage.

    Per sample:  DC block -> input gain -> asymmetric saturation -> bias shift
                 -> cathode-bypass low-shelf -> Miller-capacitance lowpass.

    The asymmetry is the heart of the "tube" character: the positive half of the
    waveform is clipped harder (tanh, models grid conduction) while the negative
    half compresses more gently (a scaled atan, models plate saturation). Blending
    the two generates even-order harmonics (2nd/4th) for warmth instead of the
    purely odd-harmonic fizz a symmetric clipper produces.
*/
class TubeStage
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        dcBlock.setCutoff (fs, 12.0);
        // Miller / interstage lowpass — rolls off ultrasonic content per stage.
        miller = Biquad::makeLowpass (fs, 12000.0, 0.707);
        // Cathode-bypass capacitor gives a low-mid lift as gain rises.
        cathode = Biquad::makeLowShelf (fs, 250.0, 0.7, 2.5);
        reset();
    }

    void reset() noexcept
    {
        dcBlock.reset();
        miller.reset();
        cathode.reset();
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
        x *= drive;

        // Asymmetric waveshaper around a shifted operating point.
        const float shifted = x + bias;
        float y;
        if (shifted >= 0.0f)
            y = std::tanh (shifted);                 // grid side: harder knee
        else
            y = 0.7f * std::atan (1.3f * shifted);   // plate side: softer, asymmetric

        y -= bias * 0.8f; // re-centre so DC offset stays small before the blocker

        y = cathode.processSample (y);
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
    Biquad    cathode;
    Biquad    miller;
};
} // namespace apex
