#pragma once

// Synthetic guitar signals and measurement helpers for the pitch-engine tests.
// A Karplus-Strong string with pick-position comb and a resonant "pickup"
// filter gives a repeatable, DI-like source whose correct answer at any tuning
// can be rendered directly -- the ground truth for a pitch shifter.

#include "apex/dsp/Fft.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace apex::test
{

constexpr double pi = 3.14159265358979323846;

struct Note
{
    double frequency;   // Hz
    double start;       // seconds
    double length;      // seconds before the string is damped
    bool   palmMute = false;
    float  velocity = 1.0f;
};

/** RBJ biquad (transposed direct form II). */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    static Biquad lowpass (double fs, double f, double q)
    {
        const double w = 2.0 * pi * f / fs, alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        const double a0 = 1.0 + alpha;
        Biquad b;
        b.b0 = (1.0 - c) / 2.0 / a0; b.b1 = (1.0 - c) / a0; b.b2 = b.b0;
        b.a1 = -2.0 * c / a0;        b.a2 = (1.0 - alpha) / a0;
        return b;
    }

    static Biquad bandpass (double fs, double f, double q)
    {
        const double w = 2.0 * pi * f / fs, alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        const double a0 = 1.0 + alpha;
        Biquad b;
        b.b0 = alpha / a0; b.b1 = 0.0; b.b2 = -alpha / a0;
        b.a1 = -2.0 * c / a0; b.a2 = (1.0 - alpha) / a0;
        return b;
    }

    float process (float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
};

/** Renders a sequence of plucked notes through a ~2.8 kHz resonant pickup. */
inline std::vector<float> renderGuitar (const std::vector<Note>& notes, double fs, double seconds,
                                        std::uint32_t seed = 1234u)
{
    std::vector<float> out ((size_t) (seconds * fs), 0.0f);
    std::uint32_t rng = seed;
    auto noise = [&rng]() { rng = rng * 1664525u + 1013904223u; return (float) ((rng >> 8) & 0xFFFF) / 32768.0f - 1.0f; };

    for (const auto& note : notes)
    {
        const double loop = fs / note.frequency - 0.5;         // averaging filter adds half a sample
        const int period  = (int) std::ceil (loop);
        const auto start  = (size_t) (note.start * fs);
        const auto stop   = (size_t) ((note.start + note.length) * fs);
        const auto end    = std::min (out.size(), stop + (size_t) (0.08 * fs));

        const double t60  = note.palmMute ? 0.18 : 4.0;
        const double gain = std::pow (10.0, -3.0 / (t60 * note.frequency));
        const double dampedGain = std::pow (10.0, -3.0 / (0.03 * note.frequency));

        // Excitation: one period of pick noise, low-passed (darker when muted)
        // and comb-filtered for a pick position 13 % along the string.
        std::vector<float> excitation ((size_t) period, 0.0f);
        float lp = 0.0f;
        const float brightness = note.palmMute ? 0.35f : 0.6f;
        for (auto& e : excitation) { lp += brightness * (noise() - lp); e = lp; }
        const int comb = std::max (1, (int) (0.13 * period));
        for (int i = period - 1; i >= comb; --i)
            excitation[(size_t) i] -= excitation[(size_t) (i - comb)];

        std::vector<float> y (end - std::min (end, start) + 4, 0.0f);
        for (size_t n = 0; start + n < end; ++n)
        {
            double fb = 0.0;
            const double pos = (double) n - loop;
            if (pos >= 1.0)
            {
                const auto i = (size_t) pos;
                const double frac = pos - (double) i;
                const double d0 = y[i] + frac * (y[i + 1] - y[i]);
                const double d1 = y[i - 1] + frac * (y[i] - y[i - 1]);
                fb = 0.5 * (d0 + d1) * (start + n < stop ? gain : dampedGain);
            }
            y[n] = (float) ((n < (size_t) period ? excitation[n] : 0.0f) + fb);
            out[start + n] += note.velocity * y[n];
        }
    }

    auto pickup = Biquad::lowpass (fs, 2800.0, 2.5);
    float peak = 1.0e-9f;
    for (auto& s : out) { s = pickup.process (s); peak = std::max (peak, std::abs (s)); }
    for (auto& s : out) s *= 0.5f / peak;
    return out;
}

/** Band-limited pulse train through a fixed resonance: a clean "formant" probe. */
inline std::vector<float> renderFormantProbe (double fs, double f0, double formantHz, double seconds)
{
    std::vector<float> out ((size_t) (seconds * fs));
    const int harmonics = (int) (0.45 * fs / f0);
    for (size_t n = 0; n < out.size(); ++n)
    {
        double s = 0.0;
        for (int h = 1; h <= harmonics; ++h)
            s += std::sin (2.0 * pi * f0 * h * (double) n / fs);
        out[n] = (float) (s / harmonics);
    }
    auto resonance = Biquad::bandpass (fs, formantHz, 3.0);
    auto lowpass   = Biquad::lowpass (fs, formantHz * 1.6, 0.7);
    float peak = 1.0e-9f;
    for (auto& s : out) { s = lowpass.process (resonance.process (s)); peak = std::max (peak, std::abs (s)); }
    for (auto& s : out) s *= 0.5f / peak;
    return out;
}

/** YIN fundamental estimate over x[start, start + length). Returns 0 if unvoiced. */
inline double estimatePitch (const std::vector<float>& x, size_t start, size_t length, double fs,
                             double fMin = 30.0, double fMax = 1500.0)
{
    const int maxLag = (int) (fs / fMin), minLag = std::max (2, (int) (fs / fMax));
    if (start + length > x.size() || (int) length <= maxLag + 64)
        return 0.0;
    const int w = (int) length - maxLag;

    std::vector<double> d ((size_t) maxLag + 2, 0.0), cmnd ((size_t) maxLag + 2, 1.0);
    double running = 0.0;
    for (int tau = 1; tau <= maxLag + 1; ++tau)
    {
        double acc = 0.0;
        for (int j = 0; j < w; ++j)
        {
            const double diff = x[start + (size_t) j] - x[start + (size_t) (j + tau)];
            acc += diff * diff;
        }
        d[(size_t) tau] = acc;
        running += acc;
        cmnd[(size_t) tau] = running > 0.0 ? acc * tau / running : 1.0;
    }

    int tau = -1;
    for (int t = minLag; t <= maxLag; ++t)
    {
        if (cmnd[(size_t) t] < 0.12)
        {
            while (t + 1 <= maxLag && cmnd[(size_t) t + 1] < cmnd[(size_t) t])
                ++t;
            tau = t;
            break;
        }
    }
    if (tau < 0)
        return 0.0;

    const double a = d[(size_t) tau - 1], b = d[(size_t) tau], c = d[(size_t) tau + 1];
    const double denom = a - 2.0 * b + c;
    const double offset = std::abs (denom) > 1.0e-12 ? 0.5 * (a - c) / denom : 0.0;
    return fs / ((double) tau + std::clamp (offset, -1.0, 1.0));
}

/** First sample in [from, to) whose magnitude reaches `fraction` of the window's peak. */
inline double onsetTime (const std::vector<float>& x, size_t from, size_t to, double fs, float fraction = 0.25f)
{
    to = std::min (to, x.size());
    float peak = 0.0f;
    for (size_t i = from; i < to; ++i)
        peak = std::max (peak, std::abs (x[i]));
    for (size_t i = from; i < to; ++i)
        if (std::abs (x[i]) >= fraction * peak)
            return (double) i / fs;
    return -1.0;
}

/** Long-term average power spectrum (linear), 2^order-point Hann frames. */
inline std::vector<double> averageSpectrum (const std::vector<float>& x, size_t start, size_t end, int order = 13)
{
    apex::dsp::Fft fft;
    fft.prepare (order);
    const int n = 1 << order;
    std::vector<apex::dsp::Fft::Complex> buf ((size_t) n);
    std::vector<double> power ((size_t) n / 2 + 1, 0.0);
    int frames = 0;

    for (size_t s = start; s + (size_t) n <= std::min (end, x.size()); s += (size_t) n / 2, ++frames)
    {
        for (int i = 0; i < n; ++i)
            buf[(size_t) i] = { x[s + (size_t) i] * (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / n)), 0.0f };
        fft.forward (buf.data());
        for (int k = 0; k <= n / 2; ++k)
            power[(size_t) k] += std::norm (buf[(size_t) k]);
    }
    for (auto& p : power)
        p /= std::max (1, frames);
    return power;
}

/** Frequency of the strongest smoothed spectral region in [lo, hi] Hz. */
inline double envelopePeak (const std::vector<double>& power, double fs, double lo, double hi, double smoothHz = 300.0)
{
    const int n = (int) (power.size() - 1) * 2;
    const double binHz = fs / n;
    const int half = std::max (1, (int) (smoothHz / binHz / 2.0));
    double best = -1.0, bestHz = 0.0;

    for (int k = (int) (lo / binHz); k <= (int) (hi / binHz) && k < (int) power.size(); ++k)
    {
        double acc = 0.0;
        int count = 0;
        for (int j = std::max (0, k - half); j <= std::min ((int) power.size() - 1, k + half); ++j, ++count)
            acc += power[(size_t) j];
        if (acc / count > best) { best = acc / count; bestHz = k * binHz; }
    }
    return bestHz;
}

/** RMS difference in dB between two spectra over third-octave bands 63 Hz .. 8 kHz,
    after matching overall level. Lower = closer timbre. */
inline double thirdOctaveDistance (const std::vector<double>& a, const std::vector<double>& b, double fs)
{
    const int n = (int) (a.size() - 1) * 2;
    const double binHz = fs / n;
    std::vector<double> diffs;

    for (double centre = 63.0; centre <= 8000.0; centre *= std::pow (2.0, 1.0 / 3.0))
    {
        const int k0 = std::max (1, (int) (centre / std::pow (2.0, 1.0 / 6.0) / binHz));
        const int k1 = std::min ((int) a.size() - 1, (int) (centre * std::pow (2.0, 1.0 / 6.0) / binHz));
        double ea = 1.0e-20, eb = 1.0e-20;
        for (int k = k0; k <= k1; ++k) { ea += a[(size_t) k]; eb += b[(size_t) k]; }
        diffs.push_back (10.0 * std::log10 (ea / eb));
    }

    double mean = 0.0;
    for (double d : diffs) mean += d;
    mean /= (double) diffs.size();

    double acc = 0.0;
    for (double d : diffs) acc += (d - mean) * (d - mean);
    return std::sqrt (acc / (double) diffs.size());
}

} // namespace apex::test
