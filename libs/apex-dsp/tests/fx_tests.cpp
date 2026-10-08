// Offline tests for the apex-dsp rig effects: ChugShaper, LowDirt,
// StereoDelay and AbyssReverb.
//
//   apex_fx_tests [output-dir]
//
// Hard checks (exit code != 0 on failure). Listening files are written to
// output-dir when one is given.

#include "apex/dsp/AbyssReverb.h"
#include "apex/dsp/ChugShaper.h"
#include "apex/dsp/FizzTamer.h"
#include "apex/dsp/LowDirt.h"
#include "apex/dsp/StereoDelay.h"
#include "TestSignals.h"
#include "WavIO.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
 #include <xmmintrin.h>
 #define APEX_FTZ() _mm_setcsr (_mm_getcsr() | 0x8040)
#else
 #define APEX_FTZ() ((void) 0)
#endif

using namespace apex;

namespace
{

constexpr double fs = 48000.0;
int failures = 0;
std::string outDir;

#define CHECK(cond, ...)                                     \
    do {                                                     \
        if (! (cond)) {                                      \
            std::printf ("  FAIL: " __VA_ARGS__);            \
            std::printf ("\n");                              \
            ++failures;                                      \
        }                                                    \
    } while (false)

using Signal = std::vector<float>;

size_t at (double seconds) { return (size_t) (seconds * fs); }

double energy (const Signal& x, size_t from, size_t to)
{
    double acc = 1.0e-30;
    for (size_t i = from; i < std::min (to, x.size()); ++i)
        acc += (double) x[i] * x[i];
    return acc;
}

double db (double powerRatio) { return 10.0 * std::log10 (powerRatio); }

float peak (const Signal& x, size_t from = 0, size_t to = (size_t) -1)
{
    float p = 0.0f;
    for (size_t i = from; i < std::min (to, x.size()); ++i)
        p = std::max (p, std::abs (x[i]));
    return p;
}

bool finite (const Signal& x)
{
    for (float v : x)
        if (! std::isfinite (v))
            return false;
    return true;
}

Signal filtered (const Signal& x, test::Biquad f)
{
    Signal y (x.size());
    for (size_t i = 0; i < x.size(); ++i)
        y[i] = f.process (x[i]);
    return y;
}

Signal sine (double hz, double seconds, float amplitude, double startSeconds = 0.0, double totalSeconds = -1.0)
{
    Signal x (at (totalSeconds > 0.0 ? totalSeconds : startSeconds + seconds), 0.0f);
    for (size_t i = at (startSeconds); i < std::min (x.size(), at (startSeconds + seconds)); ++i)
        x[i] = amplitude * (float) std::sin (2.0 * test::pi * hz * (double) i / fs);
    return x;
}

Signal noise (double seconds, float amplitude, std::uint32_t seed = 99u)
{
    Signal x (at (seconds));
    for (auto& v : x)
    {
        seed = seed * 1664525u + 1013904223u;
        v = amplitude * ((float) ((seed >> 8) & 0xFFFF) / 32768.0f - 1.0f);
    }
    return x;
}

void writeListening (const std::string& name, const Signal& l, const Signal* r = nullptr)
{
    if (outDir.empty())
        return;
    test::Audio a;
    a.sampleRate = fs;
    a.channels = { l };
    if (r != nullptr)
        a.channels.push_back (*r);
    test::writeWav (outDir + "/" + name + ".wav", a);
}

// Runs a stereo effect over (l, r) in blocks; `control` is called before every
// block with the block's start time in seconds.
template <typename Effect>
void runStereo (Effect& fx, Signal& l, Signal& r, int block,
                const std::function<void (double)>& control = {})
{
    for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, l.size() - pos);
        if (control)
            control ((double) pos / fs);
        fx.process (l.data() + pos, r.data() + pos, n);
    }
}

//==============================================================================
// A chug riff, and a stand-in for a high-gain amp: heavy symmetric clipping
// after a tight pre-filter, i.e. the dynamics are mostly gone afterwards.
std::vector<test::Note> chugRiff (double& longNoteStart)
{
    std::vector<test::Note> notes;
    const double e = 82.41, sixteenth = 60.0 / 140.0 / 4.0;
    const int pattern[] = { 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0 };
    for (int bar = 0; bar < 2; ++bar)
        for (int i = 0; i < 16; ++i)
            if (pattern[i])
                notes.push_back ({ e, (bar * 16 + i) * sixteenth, sixteenth * 0.9, true, 0.9f });
    longNoteStart = 32 * sixteenth;
    notes.push_back ({ e, longNoteStart, 1.4, false, 1.0f });
    return notes;
}

Signal fakeAmp (const Signal& di)
{
    auto pre = test::Biquad::bandpass (fs, 900.0, 0.5);
    Signal y (di.size());
    for (size_t i = 0; i < di.size(); ++i)
        y[i] = 0.5f * std::tanh (30.0f * (pre.process (di[i]) + 0.3f * di[i]));
    return y;
}

Signal runChug (const Signal& di, const Signal& amp, float amount, float hz, int block, int delay = 0)
{
    dsp::ChugShaper shaper;
    shaper.prepare (fs, block);
    shaper.setAmount (amount);
    shaper.setFrequency (hz);
    shaper.setDetectorDelay (delay);
    shaper.reset();
    Signal y = amp;
    for (size_t pos = 0; pos < y.size(); pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, y.size() - pos);
        shaper.analyse (di.data() + pos, n);
        shaper.process (y.data() + pos, n);
    }
    return y;
}

void testChug()
{
    std::printf ("[chug]\n");
    double longStart = 0.0;
    const auto notes = chugRiff (longStart);
    const auto di  = test::renderGuitar (notes, fs, longStart + 1.8);
    const auto amp = fakeAmp (di);

    const auto off = runChug (di, amp, 0.0f, 800.0f, 128);
    CHECK (off == amp, "amount 0 is not bit-exact bypass");

    const float hz = 800.0f;
    const auto on = runChug (di, amp, 1.0f, hz, 128);
    CHECK (finite (on), "non-finite output");

    // Band energy around the chosen frequency right after each palm mute vs
    // in the middle of the held note.
    const auto band = test::Biquad::bandpass (fs, hz, 1.0);
    const auto bandOn = filtered (on, band), bandOff = filtered (amp, band);
    double attackGain = 0.0;
    int count = 0;
    for (const auto& n : notes)
        if (n.palmMute)
        {
            const size_t a = at (n.start + 0.002), b = at (n.start + 0.030);
            attackGain += db (energy (bandOn, a, b) / energy (bandOff, a, b));
            ++count;
        }
    attackGain /= std::max (1, count);
    const double sustainGain = db (energy (bandOn, at (longStart + 0.5), at (longStart + 1.2))
                                   / energy (bandOff, at (longStart + 0.5), at (longStart + 1.2)));

    const auto lows = test::Biquad::lowpass (fs, 70.0, 0.7);
    const double lowGain = db (energy (filtered (on, lows), 0, on.size()) / energy (filtered (amp, lows), 0, amp.size()));

    std::printf ("  band +%.1f dB on attacks, %+.2f dB on the held note, lows %+.2f dB\n", attackGain, sustainGain, lowGain);
    CHECK (attackGain > 5.0, "attacks only boosted by %.1f dB", attackGain);
    CHECK (std::abs (sustainGain) < 1.0, "held note changed by %.2f dB", sustainGain);
    CHECK (std::abs (lowGain) < 0.5, "lows changed by %.2f dB", lowGain);

    // Same result for any block size.
    const auto other = runChug (di, amp, 1.0f, hz, 333);
    CHECK (other == on, "output depends on the block size");

    // A detector delay lines the punch up with a delayed amp signal.
    const int d = 37;
    Signal ampLate (amp.size() + (size_t) d, 0.0f), diPadded (di);
    std::copy (amp.begin(), amp.end(), ampLate.begin() + d);
    diPadded.resize (ampLate.size(), 0.0f);
    const auto late = runChug (diPadded, ampLate, 1.0f, hz, 128, d);
    float worst = 0.0f;
    for (size_t i = 0; i < on.size(); ++i)
        worst = std::max (worst, std::abs (late[i + (size_t) d] - on[i]));
    CHECK (worst < 1.0e-5f, "detector delay misaligned (error %g)", worst);

    writeListening ("chug_off", amp);
    writeListening ("chug_on", on);
}

//==============================================================================
Signal runDirt (Signal x, float amount, int block = 128)
{
    dsp::LowDirt dirt;
    dirt.prepare (fs);
    dirt.setAmount (amount);
    dirt.reset();
    for (size_t pos = 0; pos < x.size(); pos += (size_t) block)
        dirt.process (x.data() + pos, (int) std::min ((size_t) block, x.size() - pos));
    return x;
}

double bandPower (const std::vector<double>& spectrum, double hz, double width = 6.0)
{
    const double binHz = fs / ((double) (spectrum.size() - 1) * 2.0);
    double acc = 1.0e-30;
    for (int k = (int) ((hz - width) / binHz); k <= (int) ((hz + width) / binHz); ++k)
        if (k >= 0 && k < (int) spectrum.size())
            acc += spectrum[(size_t) k];
    return acc;
}

void testLowDirt()
{
    std::printf ("\n[low dirt]\n");
    const auto riff = test::renderGuitar ({ { 41.2, 0.0, 1.0 }, { 41.2, 1.2, 1.0, true } }, fs, 2.5);
    CHECK (runDirt (riff, 0.0f) == riff, "amount 0 is not bit-exact bypass");

    const auto silence = runDirt (Signal (at (1.0), 0.0f), 1.0f);
    CHECK (peak (silence) == 0.0f, "silence in, %g out", peak (silence));

    const auto low = sine (55.0, 2.0, 0.3f);
    const auto dirty = runDirt (low, 1.0f);
    const auto spectrum = test::averageSpectrum (dirty, at (0.5), at (2.0), 14);
    const double h2 = db (bandPower (spectrum, 110.0) / bandPower (spectrum, 55.0));
    const double h3 = db (bandPower (spectrum, 165.0) / bandPower (spectrum, 55.0));
    double highs = 1.0e-30, total = 1.0e-30;
    const double binHz = fs / ((double) (spectrum.size() - 1) * 2.0);
    for (size_t k = 0; k < spectrum.size(); ++k)
    {
        total += spectrum[k];
        if ((double) k * binHz > 5000.0)
            highs += spectrum[k];
    }
    std::printf ("  55 Hz: 2nd %+.1f dB, 3rd %+.1f dB, above 5 kHz %+.1f dB\n", h2, h3, db (highs / total));
    CHECK (h2 > -30.0 && h3 > -30.0, "no growl harmonics (%.1f / %.1f dB)", h2, h3);
    CHECK (db (highs / total) < -50.0, "fizz above 5 kHz at %.1f dB", db (highs / total));

    const auto high = sine (3000.0, 1.0, 0.5f);
    const auto highOut = runDirt (high, 1.0f);
    float diff = 0.0f;
    for (size_t i = 0; i < high.size(); ++i)
        diff = std::max (diff, std::abs (highOut[i] - high[i]));
    CHECK (diff < 0.01f, "highs changed by %g", diff);

    const auto loud = runDirt (riff, 1.0f);
    CHECK (finite (loud) && peak (loud) < 2.5f * peak (riff), "level blew up (%g vs %g)", peak (loud), peak (riff));
    CHECK (runDirt (riff, 1.0f, 333) == loud, "output depends on the block size");
    writeListening ("dirt_on", loud);
}

//==============================================================================
void testDelay()
{
    std::printf ("\n[delay]\n");

    auto make = [] (dsp::StereoDelay& d, bool on, double ms, float fb, float mix)
    {
        d.setEnabled (on);
        d.setTime (ms);
        d.setFeedback (fb / 0.95f);
        d.setMix (mix);
        d.setTone (1.0f);
        d.setPingPong (true);
        d.prepare (fs, 512);
    };

    // Ping-pong echoes: L at T, R at 2T, ~fb lower.
    {
        dsp::StereoDelay d;
        make (d, true, 250.0, 0.5f, 1.0f);
        Signal l (at (1.2), 0.0f), r (at (1.2), 0.0f);
        l[0] = r[0] = 1.0f;
        runStereo (d, l, r, 128);

        auto peakAt = [] (const Signal& x, size_t from, size_t to)
        {
            size_t best = from;
            for (size_t i = from; i < to; ++i)
                if (std::abs (x[i]) > std::abs (x[best])) best = i;
            return best;
        };
        const size_t t = at (0.25);
        const size_t first = peakAt (l, 100, at (0.4)), second = peakAt (r, 100, at (0.7));
        const double ratio = energy (r, second - 400, second + 400) / energy (l, first - 400, first + 400);
        std::printf ("  first echo L %+d samples, second R %+d samples, energy ratio %.2f\n",
                     (int) first - (int) t, (int) second - (int) (2 * t), ratio);
        CHECK (std::abs ((int) first - (int) t) <= 16, "first echo at %zu, expected %zu", first, t);
        CHECK (std::abs ((int) second - (int) (2 * t)) <= 32, "second echo at %zu, expected %zu", second, 2 * t);
        CHECK (energy (r, 1, at (0.4)) < 1.0e-6, "right channel echoes before 2T");
        CHECK (ratio > 0.08 && ratio < 0.3, "second echo energy ratio %.2f", ratio);
    }

    // Off from the start / mix 0: bit-exact pass-through.
    {
        const auto x = noise (0.5, 0.5f);
        for (int variant = 0; variant < 2; ++variant)
        {
            dsp::StereoDelay d;
            make (d, variant == 1, 120.0, 0.6f, variant == 0 ? 0.5f : 0.0f);
            Signal l = x, r = x;
            runStereo (d, l, r, 128);
            CHECK (l == x && r == x, "%s is not a clean pass-through", variant == 0 ? "off" : "mix 0");
        }
    }

    // Spillover: switching off keeps the repeats already in the delay, and
    // nothing played afterwards gets repeated.
    {
        dsp::StereoDelay d;
        make (d, true, 250.0, 0.5f, 1.0f);
        Signal l (at (1.4), 0.0f), r (at (1.4), 0.0f);
        l[0] = r[0] = 1.0f;
        l[at (0.6)] = r[at (0.6)] = 1.0f;
        runStereo (d, l, r, 64, [&] (double t) { d.setEnabled (t < 0.3); });
        const double tail = energy (r, at (0.45), at (0.55));
        std::printf ("  spillover tail %.3g after switching off\n", tail);
        CHECK (tail > 0.02, "switching off cut the tail (%.3g)", tail);
        CHECK (energy (l, at (0.82), at (0.88)) < 1.0e-4, "a note played while off was repeated");
    }

    // Block-size invariance and stability at full feedback with a loud input.
    {
        const auto x = noise (6.0, 0.9f);
        Signal a = x, b = x, c = x, e = x;
        for (int variant = 0; variant < 2; ++variant)
        {
            dsp::StereoDelay d;
            make (d, true, 333.0, 0.95f, 1.0f);
            auto& l = variant == 0 ? a : c;
            auto& r = variant == 0 ? b : e;
            // switch at a sample both block sizes start a block on (64 * 517 * 4)
            runStereo (d, l, r, variant == 0 ? 64 : 517, [&] (double t) { d.setTime (t * fs < 132352.0 ? 333.0 : 180.0); });
        }
        CHECK (a == c && b == e, "output depends on the block size");
        CHECK (finite (a) && peak (a) < 8.0f && peak (b) < 8.0f, "unstable at full feedback (peak %g)", std::max (peak (a), peak (b)));
    }

    // Time changes crossfade instead of jumping.
    {
        dsp::StereoDelay d;
        make (d, true, 300.0, 0.4f, 1.0f);
        Signal l = sine (220.0, 3.0, 0.3f), r = l;
        runStereo (d, l, r, 128, [&] (double t) { d.setTime (t < 1.5 ? 300.0 : 410.0); });
        auto maxStep = [&] (size_t from, size_t to)
        {
            float m = 0.0f;
            for (size_t i = from; i < to; ++i)
                m = std::max (m, std::abs (l[i] - l[i - 1]));
            return m;
        };
        const float steady = maxStep (at (1.0), at (1.5)), change = maxStep (at (1.5), at (1.7));
        std::printf ("  largest step: steady %.4f, during a time change %.4f\n", steady, change);
        CHECK (change < 1.5f * steady, "time change clicks (%.4f vs %.4f)", change, steady);
    }

    // Ducking holds the repeats down while playing.
    {
        double wetLevel[2] {};
        for (int duck = 0; duck < 2; ++duck)
        {
            dsp::StereoDelay d;
            make (d, true, 200.0, 0.3f, 1.0f);
            d.setDuck ((float) duck);
            const auto x = sine (330.0, 2.0, 0.5f);
            Signal l = x, r = x;
            runStereo (d, l, r, 128);
            for (size_t i = 0; i < l.size(); ++i) l[i] -= x[i];
            wetLevel[duck] = energy (l, at (1.0), at (2.0));
        }
        std::printf ("  ducking while playing: %.1f dB\n", db (wetLevel[1] / wetLevel[0]));
        CHECK (db (wetLevel[1] / wetLevel[0]) < -12.0, "duck only %.1f dB", db (wetLevel[1] / wetLevel[0]));
    }
}

//==============================================================================
double measureRt60 (const Signal& l, const Signal& r, size_t start)
{
    // Schroeder backward integration, T30 fit (-5 .. -35 dB) scaled to 60 dB.
    std::vector<double> edc (l.size() - start, 0.0);
    double acc = 0.0;
    for (size_t i = l.size(); i-- > start;)
    {
        acc += (double) l[i] * l[i] + (double) r[i] * r[i];
        edc[i - start] = acc;
    }
    const double total = edc[0];
    double t5 = -1.0, t35 = -1.0;
    for (size_t i = 0; i < edc.size(); ++i)
    {
        const double level = db (edc[i] / total);
        if (t5 < 0.0 && level <= -5.0) t5 = (double) i / fs;
        if (t35 < 0.0 && level <= -35.0) { t35 = (double) i / fs; break; }
    }
    return t35 > 0.0 ? 2.0 * (t35 - t5) : -1.0;
}

void runReverb (dsp::AbyssReverb& v, Signal& l, Signal& r, int block, bool on, float decay, float abyss, float mix,
                const std::function<void (double)>& control = {})
{
    v.setEnabled (on);
    v.setDecay (decay);
    v.setAbyss (abyss);
    v.setMix (mix);
    v.prepare (fs, block);
    runStereo (v, l, r, block, control);
}

void testReverb()
{
    std::printf ("\n[reverb]\n");

    // Decay time and stereo width from an impulse.
    for (float target : { 1.5f, 5.0f })
    {
        dsp::AbyssReverb v;
        const double seconds = target * 1.6 + 0.5;
        Signal l (at (seconds), 0.0f), r (at (seconds), 0.0f);
        l[0] = 1.0f;
        runReverb (v, l, r, 256, true, target, 0.0f, 1.0f);
        l[0] -= 1.0f;
        const double rt = measureRt60 (l, r, at (0.03));

        double lr = 0.0, ll = 1.0e-30, rr = 1.0e-30;
        for (size_t i = at (0.1); i < at (0.6); ++i)
        {
            lr += (double) l[i] * r[i];
            ll += (double) l[i] * l[i];
            rr += (double) r[i] * r[i];
        }
        const double correlation = lr / std::sqrt (ll * rr);
        std::printf ("  decay %.1f s: measured RT60 %.2f s, L/R correlation %+.2f\n", target, rt, correlation);
        CHECK (rt > 0.85 * target && rt < 1.15 * target, "RT60 %.2f s for a %.1f s setting", rt, target);
        CHECK (std::abs (correlation) < 0.3, "tail is not wide (correlation %.2f)", correlation);
        if (target > 2.0f)
            writeListening ("reverb_impulse", l, &r);
    }

    // Off from the start / mix 0: bit-exact pass-through.
    {
        const auto x = noise (0.5, 0.5f);
        for (int variant = 0; variant < 2; ++variant)
        {
            dsp::AbyssReverb v;
            Signal l = x, r = x;
            runReverb (v, l, r, 128, variant == 1, 3.0f, 0.5f, variant == 0 ? 0.5f : 0.0f);
            CHECK (l == x && r == x, "%s is not a clean pass-through", variant == 0 ? "off" : "mix 0");
        }
    }

    // Spillover.
    {
        dsp::AbyssReverb v;
        Signal l = sine (440.0, 0.2, 0.5f, 0.0, 2.0), r = l;
        const Signal dry = l;
        runReverb (v, l, r, 64, true, 3.0f, 0.0f, 1.0f, [&] (double t) { v.setEnabled (t < 0.25); });
        for (size_t i = 0; i < l.size(); ++i) l[i] -= dry[i];
        const double tail = energy (l, at (0.6), at (1.0));
        std::printf ("  spillover tail after switching off: %.1f dB\n", db (tail / energy (l, at (0.2), at (0.25))));
        CHECK (tail > 1.0e-3, "switching off cut the tail");
    }

    // Abyss: the tail gains an octave below the note.
    {
        double sub[2] {}, note[2] {};
        Signal kept[2];
        for (int variant = 0; variant < 2; ++variant)
        {
            dsp::AbyssReverb v;
            Signal l = sine (440.0, 0.4, 0.5f, 0.0, 4.0), r = l;
            const Signal dry = l;
            runReverb (v, l, r, 256, true, 6.0f, variant == 0 ? 0.0f : 1.0f, 1.0f);
            for (size_t i = 0; i < l.size(); ++i) l[i] -= dry[i];
            const auto spectrum = test::averageSpectrum (l, at (0.8), at (4.0), 14);
            sub[variant] = bandPower (spectrum, 220.0, 12.0);
            note[variant] = bandPower (spectrum, 440.0, 12.0);
            kept[variant] = l;
        }
        const double before = db (sub[0] / note[0]), after = db (sub[1] / note[1]);
        std::printf ("  octave below the note in the tail: %.1f dB without abyss, %.1f dB with\n", before, after);
        CHECK (after - before > 15.0, "abyss adds only %.1f dB an octave down", after - before);
        writeListening ("abyss_on", kept[1]);
    }

    // Stability with the longest decay and full abyss; block-size invariance.
    {
        Signal a = noise (2.0, 0.9f), b = a;
        a.resize (at (16.0), 0.0f);
        b.resize (a.size(), 0.0f);
        Signal c = a, e = b;
        dsp::AbyssReverb v1, v2;
        runReverb (v1, a, b, 64, true, 30.0f, 1.0f, 1.0f);
        runReverb (v2, c, e, 333, true, 30.0f, 1.0f, 1.0f);
        CHECK (a == c && b == e, "output depends on the block size");
        const double early = energy (a, at (2.5), at (4.5)), late = energy (a, at (14.0), at (16.0));
        std::printf ("  30 s decay + full abyss: peak %.2f, level 12 s later %+.1f dB\n", peak (a), db (late / early));
        CHECK (finite (a) && peak (a) < 6.0f, "unstable (peak %g)", peak (a));
        CHECK (late < early, "tail grows with full abyss");
    }
}

//==============================================================================
Signal runFizz (Signal x, float amount, int block = 128)
{
    dsp::FizzTamer fizz;
    fizz.prepare (fs);
    fizz.setAmount (amount);
    fizz.reset();
    for (size_t pos = 0; pos < x.size(); pos += (size_t) block)
        fizz.process (x.data() + pos, (int) std::min ((size_t) block, x.size() - pos));
    return x;
}

double bandEnergy (const std::vector<double>& spectrum, double lo, double hi)
{
    const double binHz = fs / ((double) (spectrum.size() - 1) * 2.0);
    double acc = 1.0e-30;
    for (int k = (int) (lo / binHz); k <= (int) (hi / binHz) && k < (int) spectrum.size(); ++k)
        acc += spectrum[(size_t) k];
    return acc;
}

void testFizzTamer()
{
    std::printf ("\n[fizz tamer]\n");
    // pink-ish noise (a gentle low-pass on white) with a +14 dB resonance at 4 kHz
    Signal flat = filtered (noise (4.0, 0.3f), test::Biquad::lowpass (fs, 6000.0, 0.5));
    auto peak = test::Biquad::bandpass (fs, 4000.0, 8.0);
    Signal resonant (flat.size());
    for (size_t i = 0; i < flat.size(); ++i)
        resonant[i] = flat[i] + 4.0f * peak.process (flat[i]);

    CHECK (runFizz (resonant, 0.0f) == resonant, "amount 0 is not bit-exact bypass");

    const auto tamed = runFizz (resonant, 1.0f);
    CHECK (finite (tamed), "non-finite output");
    CHECK (runFizz (resonant, 1.0f, 333) == tamed, "output depends on the block size");

    const auto before = test::averageSpectrum (resonant, at (0.5), at (4.0), 13);
    const auto after  = test::averageSpectrum (tamed, at (0.5), at (4.0), 13);
    const double atPeak = db (bandEnergy (after, 3800.0, 4200.0) / bandEnergy (before, 3800.0, 4200.0));
    const double lowMid = db (bandEnergy (after, 600.0, 1400.0) / bandEnergy (before, 600.0, 1400.0));
    const double away   = db (bandEnergy (after, 6500.0, 8000.0) / bandEnergy (before, 6500.0, 8000.0));

    const auto flatOut = runFizz (flat, 1.0f);
    const auto fb = test::averageSpectrum (flat, at (0.5), at (4.0), 13), fa = test::averageSpectrum (flatOut, at (0.5), at (4.0), 13);
    const double flatTreble = db (bandEnergy (fa, 1800.0, 9000.0) / bandEnergy (fb, 1800.0, 9000.0));

    std::printf ("  resonance at 4 kHz %+.1f dB, 1 kHz %+.2f dB, 6.5-8 kHz %+.2f dB, flat input's treble %+.2f dB\n",
                 atPeak, lowMid, away, flatTreble);
    CHECK (atPeak < -5.0, "resonance only cut by %.1f dB", atPeak);
    CHECK (std::abs (lowMid) < 0.3, "mids changed by %.2f dB", lowMid);
    CHECK (std::abs (away) < 2.0, "treble away from the resonance changed by %.2f dB", away);
    CHECK (std::abs (flatTreble) < 1.5, "a smooth spectrum lost %.2f dB of treble", flatTreble);
}

//==============================================================================
void measureCpu()
{
    std::printf ("\n[cpu, 48 kHz stereo, 128-sample blocks]\n");
    const auto x = noise (10.0, 0.3f);
    auto time = [&] (const char* name, const std::function<void (Signal&, Signal&)>& run)
    {
        Signal l = x, r = x;
        const auto start = std::chrono::steady_clock::now();
        run (l, r);
        const double seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
        std::printf ("  %-12s %5.2f %% of one core\n", name, 100.0 * seconds / 10.0);
    };

    time ("chug", [] (Signal& l, Signal&) { l = runChug (l, l, 1.0f, 800.0f, 128); });
    time ("low dirt", [] (Signal& l, Signal&) { l = runDirt (l, 1.0f); });
    time ("fizz tamer", [] (Signal& l, Signal&) { l = runFizz (l, 1.0f); });
    time ("delay", [] (Signal& l, Signal& r)
    {
        dsp::StereoDelay d;
        d.setEnabled (true);
        d.setFeedback (0.5f);
        d.prepare (fs, 128);
        runStereo (d, l, r, 128);
    });
    time ("reverb", [] (Signal& l, Signal& r) { dsp::AbyssReverb v; runReverb (v, l, r, 128, true, 4.0f, 0.0f, 0.5f); });
    time ("reverb+abyss", [] (Signal& l, Signal& r) { dsp::AbyssReverb v; runReverb (v, l, r, 128, true, 4.0f, 1.0f, 0.5f); });
}

} // namespace

int main (int argc, char** argv)
{
    APEX_FTZ();
    outDir = argc > 1 ? argv[1] : "";

    std::printf ("apex-dsp rig effects tests (48 kHz)\n\n");
    testChug();
    testLowDirt();
    testDelay();
    testReverb();
    testFizzTamer();
    measureCpu();

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
