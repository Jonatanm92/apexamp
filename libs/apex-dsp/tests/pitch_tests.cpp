// Offline tests and measurements for the apex-dsp pitch engine.
//
//   apex_pitch_tests [output-dir]
//
// Hard checks (exit code != 0 on failure): null / latency alignment, block-size
// invariance, pitch accuracy, formant ("Body") behaviour, stability.
// Measurements (printed): onset latency, distance to a natively tuned
// ground-truth render, CPU cost. Listening files are written to output-dir.

#include "apex/dsp/PitchShifter.h"
#include "TestSignals.h"
#include "WavIO.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace apex;
using apex::dsp::PitchMode;

namespace
{

constexpr double fs = 48000.0;
int failures = 0;

#define CHECK(cond, ...)                                     \
    do {                                                     \
        if (! (cond)) {                                      \
            std::printf ("  FAIL: " __VA_ARGS__);            \
            std::printf ("\n");                              \
            ++failures;                                      \
        }                                                    \
    } while (false)

const char* modeName (PitchMode m) { return m == PitchMode::live ? "Live" : "Studio"; }

struct Settings
{
    PitchMode mode = PitchMode::studio;
    float semitones = 0.0f;
    float body = 1.0f;
    float mix = 1.0f;
    int blockSize = 128;
};

std::vector<std::vector<float>> run (const std::vector<std::vector<float>>& input, const Settings& s, int* latency = nullptr)
{
    dsp::PitchShifter shifter;
    const int channels = (int) input.size();
    shifter.prepare (fs, s.blockSize, channels);
    shifter.setMode (s.mode);
    shifter.setSemitones (s.semitones);
    shifter.setBody (s.body);
    shifter.setMix (s.mix);
    shifter.reset();
    if (latency != nullptr)
        *latency = shifter.getLatencySamples();

    auto out = input;
    const int total = (int) input[0].size();
    for (int pos = 0; pos < total; pos += s.blockSize)
    {
        const int n = std::min (s.blockSize, total - pos);
        float* ptrs[2] {};
        for (int ch = 0; ch < channels; ++ch)
            ptrs[ch] = out[(size_t) ch].data() + pos;
        shifter.process (ptrs, channels, n);
    }
    return out;
}

std::vector<float> run (const std::vector<float>& mono, const Settings& s, int* latency = nullptr)
{
    return run (std::vector<std::vector<float>> { mono }, s, latency)[0];
}

std::vector<float> padded (std::vector<float> x, double seconds)
{
    x.resize (x.size() + (size_t) (seconds * fs), 0.0f);
    return x;
}

std::vector<test::Note> riff()
{
    // Djent-style chug figure on a low E with an accented open note and a
    // ringing octave: a mix of palm mutes and sustained notes.
    std::vector<test::Note> notes;
    const double e = 82.41, sixteenth = 60.0 / 140.0 / 4.0;
    const int pattern[] = { 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0 };
    for (int bar = 0; bar < 2; ++bar)
        for (int i = 0; i < 16; ++i)
            if (pattern[i])
                notes.push_back ({ e, (bar * 16 + i) * sixteenth, sixteenth * 0.9, true, 0.9f });
    notes.push_back ({ e, 32 * sixteenth, 1.2, false, 1.0f });
    notes.push_back ({ e * 2.0, 32 * sixteenth + 0.6, 0.9, false, 0.7f });
    return notes;
}

void writeListeningFile (const std::string& dir, const std::string& name, const std::vector<float>& x)
{
    if (dir.empty())
        return;
    test::Audio a;
    a.sampleRate = fs;
    a.channels = { x };
    test::writeWav (dir + "/" + name + ".wav", a);
}

//==============================================================================
void testNullAndAlignment (PitchMode mode)
{
    const auto guitar = padded (test::renderGuitar (riff(), fs, 3.0), 0.3);
    const std::vector<std::vector<float>> stereo { guitar, guitar };

    Settings s; s.mode = mode; s.semitones = 0.0f;
    int latency = 0;
    const auto out = run (stereo, s, &latency);

    float worst = 0.0f;
    for (size_t ch = 0; ch < 2; ++ch)
        for (size_t i = (size_t) latency; i < guitar.size(); ++i)
            worst = std::max (worst, std::abs (out[ch][i] - guitar[i - (size_t) latency]));

    std::printf ("  %-6s 0 st null vs input delayed by %d samples (%.1f ms): max error %.2e\n",
                 modeName (mode), latency, 1000.0 * latency / fs, worst);
    CHECK (worst < 1.0e-4f, "%s: 0-semitone output is not the input delayed by the reported latency", modeName (mode));

    // Dry/wet must line up too: 50 % mix at 0 st is still the delayed input.
    s.mix = 0.5f;
    const auto blended = run (guitar, s);
    float worstMix = 0.0f;
    for (size_t i = (size_t) latency; i < guitar.size(); ++i)
        worstMix = std::max (worstMix, std::abs (blended[i] - guitar[i - (size_t) latency]));
    CHECK (worstMix < 1.0e-4f, "%s: dry path is not aligned with the wet path (error %.2e)", modeName (mode), worstMix);
}

void testBlockSizeInvariance (PitchMode mode)
{
    const auto guitar = test::renderGuitar (riff(), fs, 2.0);
    Settings s; s.mode = mode; s.semitones = -5.0f; s.body = 1.0f; s.mix = 0.8f;

    s.blockSize = 1;   const auto a = run (guitar, s);
    s.blockSize = 67;  const auto b = run (guitar, s);
    s.blockSize = 1024; const auto c = run (guitar, s);

    float worst = 0.0f;
    for (size_t i = 0; i < guitar.size(); ++i)
        worst = std::max ({ worst, std::abs (a[i] - b[i]), std::abs (a[i] - c[i]) });

    std::printf ("  %-6s block sizes 1 / 67 / 1024: max difference %.2e\n", modeName (mode), worst);
    CHECK (worst < 1.0e-6f, "%s: output depends on the host block size", modeName (mode));
}

void testPitchAccuracy (PitchMode mode, double toleranceCents)
{
    const double notes[] = { 82.41, 110.0, 146.83, 329.63 };
    const float shifts[] = { -12, -7, -5, -2, 2, 5, 7, 12 };

    std::printf ("  %-6s pitch error in cents (input E2 A2 D3 E4 x shift):\n", modeName (mode));
    double worst = 0.0;

    for (float st : shifts)
    {
        std::printf ("    %+3d st:", (int) st);
        for (double f0 : notes)
        {
            const auto in = test::renderGuitar ({ { f0, 0.05, 2.5, false, 1.0f } }, fs, 2.6);
            Settings s; s.mode = mode; s.semitones = st;
            int latency = 0;
            const auto out = run (in, s, &latency);

            const size_t at = (size_t) (0.6 * fs), len = 8192 + 2048;
            const double fin  = test::estimatePitch (in, at, len, fs);
            const double fout = test::estimatePitch (out, at + (size_t) latency, len, fs);
            const double cents = (fin > 0 && fout > 0) ? 1200.0 * std::log2 (fout / (fin * std::pow (2.0, st / 12.0))) : 9999.0;

            std::printf (" %7.2f", cents);
            worst = std::max (worst, std::abs (cents));
        }
        std::printf ("\n");
    }

    CHECK (worst <= toleranceCents, "%s: pitch error %.1f cents exceeds %.1f", modeName (mode), worst, toleranceCents);
}

void measureLatency (PitchMode mode)
{
    // Isolated plucks on different strings; compare onset times in and out.
    std::vector<test::Note> notes;
    const double f[] = { 82.41, 110.0, 146.83, 196.0, 61.74, 73.42 };
    for (int i = 0; i < 6; ++i)
        notes.push_back ({ f[i], 0.2 + 0.6 * i, 0.4, i % 2 == 0, 1.0f });
    const auto in = test::renderGuitar (notes, fs, 4.0);

    for (float st : { -7.0f, -2.0f, 5.0f })
    {
        Settings s; s.mode = mode; s.semitones = st;
        int reported = 0;
        const auto out = run (in, s, &reported);

        double sum = 0.0, lo = 1.0e9, hi = -1.0e9;
        for (const auto& n : notes)
        {
            const auto from = (size_t) ((n.start - 0.005) * fs);
            const double tin  = test::onsetTime (in, from, from + (size_t) (0.1 * fs), fs);
            const double tout = test::onsetTime (out, from, from + (size_t) (0.2 * fs), fs);
            const double ms = 1000.0 * (tout - tin);
            sum += ms; lo = std::min (lo, ms); hi = std::max (hi, ms);
        }
        const double mean = sum / (double) notes.size();
        std::printf ("  %-6s %+3d st onset delay: mean %5.1f ms (min %5.1f, max %5.1f), reported %5.1f ms\n",
                     modeName (mode), (int) st, mean, lo, hi, 1000.0 * reported / fs);

        if (mode == PitchMode::studio)
            CHECK (std::abs (mean - 1000.0 * reported / fs) < 4.0, "Studio: onsets drift from the reported latency");
        else
            CHECK (hi < 40.0, "Live: onset delay %.1f ms is not playable", hi);
    }
}

void testFormant (PitchMode mode)
{
    const double formant = 2500.0, st = -7.0;
    const double ratio = std::pow (2.0, st / 12.0);
    const auto in = test::renderFormantProbe (fs, 110.0, formant, 2.0);

    for (float body : { 1.0f, 0.0f })
    {
        Settings s; s.mode = mode; s.semitones = (float) st; s.body = body;
        const auto out = run (in, s);
        const auto spectrum = test::averageSpectrum (out, (size_t) (0.4 * fs), out.size());
        const double peak = test::envelopePeak (spectrum, fs, 800.0, 6000.0);
        const double expected = body > 0.5f ? formant : formant * ratio;

        std::printf ("  %-6s Body %3.0f %%: resonance at %6.0f Hz (expected ~%4.0f Hz)\n",
                     modeName (mode), body * 100.0f, peak, expected);
        CHECK (std::abs (peak / expected - 1.0) < 0.15, "%s: Body %.0f %% put the resonance at %.0f Hz instead of ~%.0f Hz",
               modeName (mode), body * 100.0f, peak, expected);
    }
}

void testStability (PitchMode mode)
{
    std::vector<float> torture ((size_t) (2.0 * fs), 0.0f);
    std::uint32_t rng = 99u;
    for (size_t i = 0; i < torture.size(); ++i)
    {
        rng = rng * 1664525u + 1013904223u;
        const float noise = (float) ((rng >> 8) & 0xFFFF) / 32768.0f - 1.0f;
        if (i < torture.size() / 4)        torture[i] = noise;                  // full-scale noise
        else if (i < torture.size() / 2)   torture[i] = 0.0f;                   // silence
        else if (i < 3 * torture.size() / 4) torture[i] = 0.9f;                 // DC
        else                               torture[i] = (i % 200 < 100) ? 1.0f : -1.0f;   // square
    }

    const int before = failures;
    for (float st : { -24.0f, -12.0f, 0.5f, 12.0f, 24.0f })
    {
        for (float body : { 0.0f, 1.0f })
        {
            Settings s; s.mode = mode; s.semitones = st; s.body = body;
            const auto out = run (torture, s);
            float peak = 0.0f;
            bool finite = true;
            for (float v : out) { finite = finite && std::isfinite (v); peak = std::max (peak, std::abs (v)); }
            CHECK (finite && peak < 8.0f, "%s %+.1f st body %.0f: unstable (finite=%d peak=%.2f)",
                   modeName (mode), st, body, finite, peak);
        }
    }
    std::printf ("  %-6s torture signals at -24..+24 st: %s\n", modeName (mode), failures == before ? "stable" : "UNSTABLE");
}

void measureGroundTruth (const std::string& dir)
{
    // The same riff rendered in E standard and natively in A: shifting the E
    // version down 7 semitones should sound like the native A version.
    const auto notesE = riff();
    auto notesA = notesE;
    for (auto& n : notesA) n.frequency *= std::pow (2.0, -7.0 / 12.0);

    const auto e = test::renderGuitar (notesE, fs, 4.5);
    const auto a = test::renderGuitar (notesA, fs, 4.5);
    const auto truth = test::averageSpectrum (a, 0, a.size());
    writeListeningFile (dir, "riff_E_standard", e);
    writeListeningFile (dir, "riff_A_native_ground_truth", a);

    std::printf ("  timbre distance to a natively tuned render (third-octave dB RMS, lower is closer):\n");
    for (auto mode : { PitchMode::live, PitchMode::studio })
    {
        for (float body : { 0.0f, 1.0f })
        {
            Settings s; s.mode = mode; s.semitones = -7.0f; s.body = body;
            int latency = 0;
            auto out = run (e, s, &latency);
            out.erase (out.begin(), out.begin() + latency);
            const double d = test::thirdOctaveDistance (test::averageSpectrum (out, 0, out.size()), truth, fs);
            std::printf ("    %-6s Body %3.0f %%: %5.2f dB\n", modeName (mode), body * 100.0f, d);
            writeListeningFile (dir, std::string ("riff_E_to_A_") + modeName (mode) + (body > 0.5f ? "_body100" : "_body0"), out);
        }
    }
}

void measureCpu()
{
    const auto guitar = test::renderGuitar (riff(), fs, 10.0);
    const std::vector<std::vector<float>> stereo { guitar, guitar };

    for (auto mode : { PitchMode::live, PitchMode::studio })
    {
        Settings s; s.mode = mode; s.semitones = -7.0f; s.body = 1.0f;
        const auto t0 = std::chrono::steady_clock::now();
        run (stereo, s);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  %-6s stereo, Body on: %.2f %% of one core in real time\n", modeName (mode), 100.0 * secs / 10.0);
    }
}

} // namespace

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : "";

    std::printf ("apex-dsp pitch engine tests (48 kHz)\n\n[null / alignment]\n");
    for (auto m : { PitchMode::live, PitchMode::studio }) testNullAndAlignment (m);

    std::printf ("\n[block-size invariance]\n");
    for (auto m : { PitchMode::live, PitchMode::studio }) testBlockSizeInvariance (m);

    std::printf ("\n[pitch accuracy]\n");
    testPitchAccuracy (PitchMode::live, 10.0);
    testPitchAccuracy (PitchMode::studio, 5.0);

    std::printf ("\n[latency]\n");
    for (auto m : { PitchMode::live, PitchMode::studio }) measureLatency (m);

    std::printf ("\n[formants / Body]\n");
    for (auto m : { PitchMode::live, PitchMode::studio }) testFormant (m);

    std::printf ("\n[stability]\n");
    for (auto m : { PitchMode::live, PitchMode::studio }) testStability (m);

    std::printf ("\n[ground truth: E standard riff -> A]\n");
    measureGroundTruth (dir);

    std::printf ("\n[cpu]\n");
    measureCpu();

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
