#pragma once

// A synthetic electric-guitar DI for demo renders: a digital waveguide string
// per note (two slightly detuned polarisations, a frequency-dependent loop
// loss so palm mutes go dark quickly while the fundamental thumps, a pick
// burst comb-filtered at the pick position, a fret-hand release), summed and
// heard through a bridge humbucker (pickup-position comb and resonance).
// Deterministic for a given seed, so takes can be re-rendered exactly.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace apex::demo
{

constexpr double pi = 3.14159265358979323846;

struct Note
{
    double hz = 46.25;
    double start = 0.0;      // seconds
    double length = 0.1;     // seconds until the fretting hand releases
    float palmMute = 1.0f;   // 0 open .. 1 tight palm mute
    float velocity = 1.0f;
};

class Rng
{
public:
    explicit Rng (std::uint32_t seed) : state (seed * 2654435761u + 1u) {}
    float uniform() { state = state * 1664525u + 1013904223u; return (float) (state >> 8) / 16777216.0f; }
    float bipolar() { return 2.0f * uniform() - 1.0f; }
    float gauss() { float s = 0.0f; for (int i = 0; i < 4; ++i) s += bipolar(); return s * 0.866f; }

private:
    std::uint32_t state;
};

struct OnePoleLow
{
    double a = 0.0, z = 0.0;
    double process (double x) { z = (1.0 - a) * x + a * z; return z; }
};

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    static Biquad lowpass (double fs, double f, double q)
    {
        const double w = 2.0 * pi * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + al;
        Biquad b;
        b.b0 = (1.0 - c) / 2.0 / a0; b.b1 = (1.0 - c) / a0; b.b2 = b.b0; b.a1 = -2.0 * c / a0; b.a2 = (1.0 - al) / a0;
        return b;
    }
    static Biquad highpass (double fs, double f, double q)
    {
        const double w = 2.0 * pi * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + al;
        Biquad b;
        b.b0 = (1.0 + c) / 2.0 / a0; b.b1 = -(1.0 + c) / a0; b.b2 = b.b0; b.a1 = -2.0 * c / a0; b.a2 = (1.0 - al) / a0;
        return b;
    }
    static Biquad peak (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w);
        const double a0 = 1.0 + al / A;
        Biquad b;
        b.b0 = (1.0 + al * A) / a0; b.b1 = -2.0 * c / a0; b.b2 = (1.0 - al * A) / a0; b.a1 = -2.0 * c / a0; b.a2 = (1.0 - al / A) / a0;
        return b;
    }
    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

/** One polarisation of a string: fractional delay loop with a one-pole loss filter. */
class StringLoop
{
public:
    StringLoop (double fs, double hz, double loopLowpass, double t60)
        : sampleRate (fs)
    {
        filter.a = loopLowpass;
        // the loop filter delays low frequencies by a/(1-a) samples; take it off the line
        const double filterDelay = loopLowpass / (1.0 - loopLowpass);
        delay = std::max (2.0, fs / hz - filterDelay);
        buffer.assign ((size_t) std::ceil (delay) + 4, 0.0);
        setDecay (hz, t60);
    }

    void setDecay (double hz, double t60) { gain = std::pow (10.0, -3.0 / (t60 * hz)); }
    void setLowpass (double a) { filter.a = a; }

    double process (double excitation)
    {
        // read `delay` samples back (linear interpolation)
        const double readPos = (double) write - delay;
        const double wrapped = readPos < 0.0 ? readPos + (double) buffer.size() : readPos;
        const auto i0 = (size_t) wrapped;
        const double frac = wrapped - (double) i0;
        const double d = buffer[i0 % buffer.size()] * (1.0 - frac) + buffer[(i0 + 1) % buffer.size()] * frac;
        const double y = excitation + gain * filter.process (d);
        buffer[write] = y;
        write = (write + 1) % buffer.size();
        return y;
    }

private:
    double sampleRate, delay = 2.0, gain = 0.99;
    OnePoleLow filter;
    std::vector<double> buffer;
    size_t write = 0;
};

/** Renders notes into a mono DI at `fs`; peak normalised to `peak`. */
inline std::vector<float> renderDI (const std::vector<Note>& notes, double fs, double seconds, std::uint32_t seed,
                                    float pickPosition = 0.11f, float peak = 0.5f)
{
    std::vector<double> mix ((size_t) (seconds * fs), 0.0);
    Rng rng (seed);

    for (const auto& n : notes)
    {
        const auto start = (size_t) std::max (0.0, n.start * fs);
        if (start >= mix.size())
            continue;
        const double pm = std::clamp ((double) n.palmMute, 0.0, 1.0);
        const double vel = std::clamp ((double) n.velocity, 0.05, 1.0);

        // open strings ring for seconds; a palm mute stops the highs at once
        const double t60 = 5.0 * (1.0 - pm) + 0.32 * pm;
        const double loopLow = 0.06 + 0.70 * pm;
        StringLoop a (fs, n.hz, loopLow, t60), b (fs, n.hz * 1.0007, loopLow * 1.04, t60 * 0.8);

        // the pluck: the string pulled into a triangle with its corner at the
        // pick (harmonics fall 12 dB an octave, with notches at multiples of
        // 1/pickPosition), the corner rounded by the pick's width, softer when
        // picked lightly or palm-muted, plus a little pick noise
        const int period = std::max (2, (int) std::lround (fs / n.hz));
        std::vector<double> burst ((size_t) period);
        OnePoleLow corner;
        // the pick rounds the corner over about 1-4 % of the string
        corner.a = std::exp (-1.0 / std::max (1.0, (0.008 + 0.02 * (1.0 - vel) + 0.02 * pm) * period));
        // (the triangle starts and ends at zero: no step for the loop to recirculate)
        for (int i = 0; i < period; ++i)
        {
            const double x = (double) i / (double) period;
            const double shape = x < pickPosition ? x / pickPosition : (1.0 - x) / (1.0 - pickPosition);
            burst[(size_t) i] = corner.process (shape + 0.002 * rng.bipolar());
        }

        const auto release = start + (size_t) (n.length * fs);
        const auto end = std::min (mix.size(), release + (size_t) (0.25 * fs));
        // a bridge humbucker senses the string 1/17 of the way along it: a comb at that fraction of the period
        const auto pickupGap = (size_t) std::max (1L, std::lround (0.06 * period));
        std::vector<double> history (end - start, 0.0);
        bool released = false;
        double scrape = 0.0, previous = 0.0;
        for (size_t t = start; t < end; ++t)
        {
            const size_t k = t - start;
            if (! released && t >= release)
            {
                // the fretting hand lets go: a fast, dark decay
                released = true;
                a.setDecay (n.hz, 0.035);
                b.setDecay (n.hz * 1.0007, 0.035);
                a.setLowpass (0.85);
                b.setLowpass (0.85);
            }
            const double e = k < (size_t) period ? burst[k] * vel : 0.0;
            const double displacement = a.process (e) + 0.35 * b.process (0.6 * e);
            // a magnetic pickup hears the string's velocity
            const double y = displacement - previous;
            previous = displacement;
            // the pick itself: a click on the string, before the string answers
            if (k < (size_t) (0.0015 * fs))
                scrape = 0.0004 * vel * rng.bipolar() * std::exp (-(double) k / (0.0004 * fs));
            else
                scrape = 0.0;
            history[k] = y;
            mix[t] += y - 0.8 * (k >= pickupGap ? history[k - pickupGap] : 0.0) + scrape;
        }
    }

    // the pickup coil's resonance and the cable's top end
    std::vector<float> out (mix.size());
    auto resonance = Biquad::peak (fs, 3000.0, 1.6, 6.0);
    auto cable = Biquad::lowpass (fs, 6500.0, 0.7);
    auto dc = Biquad::highpass (fs, 25.0, 0.7);
    double maxAbs = 1.0e-9;
    for (size_t t = 0; t < mix.size(); ++t)
    {
        const double y = dc.process (cable.process (resonance.process (mix[t])));
        out[t] = (float) y;
        maxAbs = std::max (maxAbs, std::abs (y));
    }
    // a little hum and hiss, like any real DI
    Rng hiss (seed ^ 0x5bd1e995u);
    for (size_t t = 0; t < out.size(); ++t)
    {
        out[t] = (float) (out[t] * peak / maxAbs);
        out[t] += 0.00007f * hiss.bipolar() + 0.00003f * (float) std::sin (2.0 * pi * 50.0 * (double) t / fs);
    }
    return out;
}

} // namespace apex::demo
