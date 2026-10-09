#pragma once

#include <cmath>

namespace apex::dsp
{

constexpr double kPi = 3.14159265358979323846;

/** RBJ biquad, transposed direct form II. Coefficients are designed in double
    and run in float. */
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    /** Takes another filter's coefficients but keeps this one's state. */
    void copyCoefficients (const Biquad& o) noexcept
    {
        b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2;
    }

    static Biquad lowpass (double fs, double f, double q = 0.7071)
    {
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        return make ((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
    }

    static Biquad highpass (double fs, double f, double q = 0.7071)
    {
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        return make ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
    }

    /** Band-pass with 0 dB gain (and zero phase) at the centre frequency. */
    static Biquad bandpass (double fs, double f, double q)
    {
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        return make (alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
    }

private:
    static Biquad make (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        Biquad b;
        b.b0 = (float) (nb0 / na0); b.b1 = (float) (nb1 / na0); b.b2 = (float) (nb2 / na0);
        b.a1 = (float) (na1 / na0); b.a2 = (float) (na2 / na0);
        return b;
    }
};

/** One-pole smoother / low-pass. */
struct OnePole
{
    float a = 1.0f, z = 0.0f;

    void reset (float value = 0.0f) noexcept { z = value; }
    void setCutoff (double fs, double f) noexcept { a = (float) (1.0 - std::exp (-2.0 * kPi * f / fs)); }
    void setTime (double fs, double seconds) noexcept { a = seconds > 0.0 ? (float) (1.0 - std::exp (-1.0 / (seconds * fs))) : 1.0f; }

    float process (float x) noexcept { z += a * (x - z); return z; }
};

/** Peak envelope with separate attack and release times. */
struct EnvelopeFollower
{
    float attack = 1.0f, release = 1.0f, z = 0.0f;

    void reset() noexcept { z = 0.0f; }
    void setTimes (double fs, double attackSeconds, double releaseSeconds) noexcept
    {
        attack  = (float) (1.0 - std::exp (-1.0 / (attackSeconds * fs)));
        release = (float) (1.0 - std::exp (-1.0 / (releaseSeconds * fs)));
    }

    float process (float x) noexcept
    {
        const float level = std::abs (x);
        z += (level > z ? attack : release) * (level - z);
        return z;
    }
};

/** Quadrature oscillator for slow modulation (no per-sample trig). */
struct SlowOscillator
{
    double c = 1.0, s = 0.0, x = 1.0, y = 0.0;

    void setFrequency (double fs, double hz, double phase = 0.0) noexcept
    {
        c = std::cos (2.0 * kPi * hz / fs);
        s = std::sin (2.0 * kPi * hz / fs);
        x = std::cos (phase);
        y = std::sin (phase);
    }

    float next() noexcept
    {
        const double nx = x * c - y * s;
        y = x * s + y * c;
        x = nx;
        // Renormalise slowly so rounding never grows or shrinks the amplitude.
        const double g = 1.5 - 0.5 * (x * x + y * y);
        x *= g;
        y *= g;
        return (float) y;
    }
};

} // namespace apex::dsp
