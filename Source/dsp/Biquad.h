#pragma once

#include <cmath>

namespace apex
{
// Portable pi constant. MSVC does not define M_PI unless _USE_MATH_DEFINES is
// set before <cmath>, so we define our own to keep the DSP cross-platform.
inline constexpr double kPi = 3.14159265358979323846;

/**
    Transposed Direct Form II biquad. Pure C++ (no JUCE) so it can be unit-tested
    and reused inside oversampled blocks without allocation.

    Coefficients are normalised (a0 == 1). Use the static design helpers to build
    common filter shapes from a sample rate.
*/
class Biquad
{
public:
    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float processSample (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setCoefficients (float nb0, float nb1, float nb2,
                          float na1, float na2) noexcept
    {
        b0 = nb0; b1 = nb1; b2 = nb2; a1 = na1; a2 = na2;
    }

    // ---- design helpers -------------------------------------------------
    static Biquad makeLowpass (double fs, double freq, double q)
    {
        return designRBJ (fs, freq, q, 0.0, Type::lowpass);
    }
    static Biquad makeHighpass (double fs, double freq, double q)
    {
        return designRBJ (fs, freq, q, 0.0, Type::highpass);
    }
    static Biquad makePeak (double fs, double freq, double q, double gainDb)
    {
        return designRBJ (fs, freq, q, gainDb, Type::peak);
    }
    static Biquad makeLowShelf (double fs, double freq, double q, double gainDb)
    {
        return designRBJ (fs, freq, q, gainDb, Type::lowShelf);
    }
    static Biquad makeHighShelf (double fs, double freq, double q, double gainDb)
    {
        return designRBJ (fs, freq, q, gainDb, Type::highShelf);
    }

private:
    enum class Type { lowpass, highpass, peak, lowShelf, highShelf };

    static Biquad designRBJ (double fs, double freq, double q, double gainDb, Type type)
    {
        Biquad bq;
        if (freq <= 0.0)   freq = 1.0;
        if (freq > fs * 0.49) freq = fs * 0.49; // keep below Nyquist
        if (q <= 0.0001)   q = 0.0001;

        const double A     = std::pow (10.0, gainDb / 40.0);
        const double w0    = 2.0 * kPi * freq / fs;
        const double cosw0 = std::cos (w0);
        const double sinw0 = std::sin (w0);
        const double alpha = sinw0 / (2.0 * q);

        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

        switch (type)
        {
            case Type::lowpass:
                b0 = (1.0 - cosw0) * 0.5; b1 = 1.0 - cosw0; b2 = b0;
                a0 = 1.0 + alpha; a1 = -2.0 * cosw0; a2 = 1.0 - alpha;
                break;
            case Type::highpass:
                b0 = (1.0 + cosw0) * 0.5; b1 = -(1.0 + cosw0); b2 = b0;
                a0 = 1.0 + alpha; a1 = -2.0 * cosw0; a2 = 1.0 - alpha;
                break;
            case Type::peak:
                b0 = 1.0 + alpha * A; b1 = -2.0 * cosw0; b2 = 1.0 - alpha * A;
                a0 = 1.0 + alpha / A; a1 = -2.0 * cosw0; a2 = 1.0 - alpha / A;
                break;
            case Type::lowShelf:
            {
                const double s = 2.0 * std::sqrt (A) * alpha;
                b0 =      A * ((A + 1) - (A - 1) * cosw0 + s);
                b1 =  2 * A * ((A - 1) - (A + 1) * cosw0);
                b2 =      A * ((A + 1) - (A - 1) * cosw0 - s);
                a0 =           (A + 1) + (A - 1) * cosw0 + s;
                a1 =     -2 * ((A - 1) + (A + 1) * cosw0);
                a2 =           (A + 1) + (A - 1) * cosw0 - s;
                break;
            }
            case Type::highShelf:
            {
                const double s = 2.0 * std::sqrt (A) * alpha;
                b0 =      A * ((A + 1) + (A - 1) * cosw0 + s);
                b1 = -2 * A * ((A - 1) + (A + 1) * cosw0);
                b2 =      A * ((A + 1) + (A - 1) * cosw0 - s);
                a0 =           (A + 1) - (A - 1) * cosw0 + s;
                a1 =      2 * ((A - 1) - (A + 1) * cosw0);
                a2 =           (A + 1) - (A - 1) * cosw0 - s;
                break;
            }
        }

        const double inv = 1.0 / a0;
        bq.setCoefficients (float (b0 * inv), float (b1 * inv), float (b2 * inv),
                            float (a1 * inv), float (a2 * inv));
        return bq;
    }

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};

/** First-order DC blocker (highpass at a few Hz). */
class DCBlocker
{
public:
    void setCutoff (double fs, double freq) noexcept
    {
        R = float (1.0 - (2.0 * kPi * freq / fs));
    }
    void reset() noexcept { x1 = y1 = 0.0f; }

    inline float processSample (float x) noexcept
    {
        const float y = x - x1 + R * y1;
        x1 = x; y1 = y;
        return y;
    }

private:
    float R = 0.9995f, x1 = 0.0f, y1 = 0.0f;
};

/** Simple one-pole envelope follower with independent attack/release. */
class EnvelopeFollower
{
public:
    void prepare (double fs) noexcept { sampleRate = fs; updateCoeffs(); }

    void setTimes (float attackMs, float releaseMs) noexcept
    {
        atkMs = attackMs; relMs = releaseMs; updateCoeffs();
    }

    void reset() noexcept { env = 0.0f; }

    inline float processSample (float x) noexcept
    {
        const float r = std::fabs (x);
        const float c = (r > env) ? atk : rel;
        env = c * (env - r) + r;
        return env;
    }

    float getValue() const noexcept { return env; }

private:
    void updateCoeffs() noexcept
    {
        atk = std::exp (-1.0f / (float (sampleRate) * atkMs * 0.001f));
        rel = std::exp (-1.0f / (float (sampleRate) * relMs * 0.001f));
    }

    double sampleRate = 44100.0;
    float atkMs = 1.0f, relMs = 100.0f;
    float atk = 0.0f, rel = 0.0f, env = 0.0f;
};
} // namespace apex
