// Offline DSP test harness for ApexAmp.
//
// Compiles ONLY the pure-C++ DSP core (no JUCE). It synthesizes a guitar-like
// test signal (plucked, decaying, with harmonics + a palm-mute style transient),
// runs it through the full AmpCore chain for a couple of presets, checks the
// output for NaN/Inf/denormal blow-ups, prints peak/RMS, and writes WAV files
// you can listen to. This lets you iterate on the DSP in seconds.
//
// Build:  cmake --build build --target apexamp_offline_test
// Run:    ./build/apexamp_offline_test

#include "dsp/AmpCore.h"

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <limits>

namespace
{
constexpr double kSampleRate = 192000.0; // emulate the oversampled rate AmpCore runs at
constexpr double kDurationS  = 2.0;

// ---- minimal 16-bit mono WAV writer ---------------------------------------
void writeWav (const std::string& path, const std::vector<float>& samples, double fs)
{
    std::ofstream f (path, std::ios::binary);
    if (! f) { std::printf ("  ! could not open %s for writing\n", path.c_str()); return; }

    auto put32 = [&] (uint32_t v) { f.write (reinterpret_cast<const char*> (&v), 4); };
    auto put16 = [&] (uint16_t v) { f.write (reinterpret_cast<const char*> (&v), 2); };

    const uint32_t sr      = (uint32_t) fs;
    const uint16_t channels = 1;
    const uint16_t bits     = 16;
    const uint32_t byteRate = sr * channels * bits / 8;
    const uint16_t blockAlign = channels * bits / 8;
    const uint32_t dataBytes = (uint32_t) (samples.size() * 2);

    f.write ("RIFF", 4); put32 (36 + dataBytes); f.write ("WAVE", 4);
    f.write ("fmt ", 4); put32 (16); put16 (1); put16 (channels);
    put32 (sr); put32 (byteRate); put16 (blockAlign); put16 (bits);
    f.write ("data", 4); put32 (dataBytes);

    for (float s : samples)
    {
        s = std::fmax (-1.0f, std::fmin (1.0f, s));
        put16 ((int16_t) std::lround (s * 32767.0f));
    }
}

// ---- synthetic guitar-ish DI signal ---------------------------------------
std::vector<float> makeTestSignal (double fs, double seconds)
{
    const int n = (int) (fs * seconds);
    std::vector<float> out ((size_t) n, 0.0f);

    const double f0 = 82.41; // low E
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / fs;
        // pluck envelope: fast attack, exponential decay, re-pluck at 1.0s
        const double tt = std::fmod (t, 1.0);
        const double env = std::exp (-tt * 4.0) * (1.0 - std::exp (-tt * 800.0));
        double s = 0.0;
        s += 1.0  * std::sin (2.0 * M_PI * f0 * 1 * t);
        s += 0.5  * std::sin (2.0 * M_PI * f0 * 2 * t);
        s += 0.33 * std::sin (2.0 * M_PI * f0 * 3 * t);
        s += 0.25 * std::sin (2.0 * M_PI * f0 * 4 * t);
        out[(size_t) i] = (float) (0.4 * env * s);
    }
    return out;
}

struct Stats { float peak = 0.0f; float rms = 0.0f; bool finite = true; };

Stats analyse (const std::vector<float>& x)
{
    Stats st;
    double acc = 0.0;
    for (float v : x)
    {
        if (! std::isfinite (v)) st.finite = false;
        st.peak = std::fmax (st.peak, std::fabs (v));
        acc += (double) v * v;
    }
    st.rms = (float) std::sqrt (acc / std::fmax (1.0, (double) x.size()));
    return st;
}

float dB (float lin) { return 20.0f * std::log10 (std::fmax (1.0e-9f, lin)); }

void runPreset (const char* name, const apex::AmpParams& p, const std::vector<float>& dry)
{
    apex::AmpCore core;
    core.prepare (kSampleRate);
    core.setParams (p);

    std::vector<float> wet = dry;
    core.process (wet.data(), (int) wet.size());

    const Stats st = analyse (wet);
    std::printf ("  [%-14s] peak=%6.2f dB  rms=%6.2f dB  %s\n",
                 name, dB (st.peak), dB (st.rms),
                 st.finite ? "OK (finite)" : "*** NON-FINITE OUTPUT ***");

    writeWav (std::string ("out_") + name + ".wav", wet, kSampleRate);
}
} // namespace

int main()
{
    std::printf ("ApexAmp offline DSP test @ %.0f Hz\n", kSampleRate);
    std::printf ("------------------------------------------------\n");

    const auto dry = makeTestSignal (kSampleRate, kDurationS);
    {
        const Stats d = analyse (dry);
        std::printf ("  dry signal      peak=%6.2f dB  rms=%6.2f dB\n\n", dB (d.peak), dB (d.rms));
    }

    // Preset 1: tight rhythm crunch (Marshall-ish)
    {
        apex::AmpParams p;
        p.channel = apex::PreampChannel::tight;
        p.tonestack = apex::TonestackModel::marshall;
        p.gain = 0.6f; p.tight = 0.4f; p.bass = 0.55f; p.mid = 0.6f; p.treble = 0.55f;
        p.chug = 0.3f; p.sag = 0.3f; p.powerDrive = 0.4f;
        runPreset ("tight_crunch", p, dry);
    }

    // Preset 2: scooped modern metal lead
    {
        apex::AmpParams p;
        p.channel = apex::PreampChannel::scoop;
        p.tonestack = apex::TonestackModel::modernMetal;
        p.gain = 0.85f; p.push = 0.5f; p.superCut = 0.6f; p.tight = 0.6f;
        p.bass = 0.6f; p.mid = 0.35f; p.treble = 0.6f;
        p.chug = 0.6f; p.lowDirtDrive = 0.5f; p.lowDirtMix = 0.3f;
        p.sag = 0.5f; p.powerDrive = 0.6f;
        runPreset ("scoop_metal", p, dry);
    }

    // Preset 3: clean-ish Fender
    {
        apex::AmpParams p;
        p.channel = apex::PreampChannel::tight;
        p.tonestack = apex::TonestackModel::fender;
        p.gain = 0.25f; p.bass = 0.6f; p.mid = 0.45f; p.treble = 0.6f;
        p.sag = 0.2f; p.powerDrive = 0.2f;
        runPreset ("fender_clean", p, dry);
    }

    std::printf ("\nDone. WAV files written to the working directory.\n");
    return 0;
}
