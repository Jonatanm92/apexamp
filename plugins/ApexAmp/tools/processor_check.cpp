// Processor-level checks (run as `processor_check gate` / `processor_check bypass`).
//
// gate: the noise gate on fast palm-muted chugs through a cranked rig. Between
// chugs the output has to go quiet (a high-gain amp squashes a slow gate fade,
// which left the hiss between notes at almost full level), and the pick attack
// of every chug has to come through untouched.
//
// bypass: with the Drop pedal on, the bypassed output is the input delayed by
// the reported latency (a latency-compensating host keeps it in time), and the
// echo, cab and amp keep decaying while bypassed: coming back sounds exactly
// as if the input had simply stopped, never a tail frozen at the bypass.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

#include <cstdio>

namespace
{
    constexpr double fs = 48000.0;
    constexpr int block = 256;
    constexpr double noteSeconds = 0.08, restSeconds = 0.12;
    constexpr int numNotes = 10;

    void set (ApexAmpProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    double rmsDb (const std::vector<float>& x, double from, double to)
    {
        double sum = 0.0;
        const auto a = (size_t) (from * fs), b = (size_t) (to * fs);
        for (size_t i = a; i < b; ++i)
            sum += (double) x[i] * x[i];
        return 10.0 * std::log10 (sum / (double) (b - a) + 1.0e-20);
    }

    std::vector<float> render (bool gateOn)
    {
        ApexAmpProcessor proc;
        proc.setRateAndBufferSizeDetails (fs, block);
        proc.presets.loadPreset (0);
        set (proc, "rig", 0);
        set (proc, "boostOn", 1);
        set (proc, "boostDrive", 30);
        set (proc, "gateOn", gateOn ? 1.0f : 0.0f);
        set (proc, "gate", -55);
        set (proc, "gateHold", 30);
        proc.prepareToPlay (fs, block);

        // hiss at -75 dBFS, then palm-muted low-E chugs: 80 ms on, 120 ms off
        const double start = 0.3;
        const auto total = (size_t) ((start + numNotes * (noteSeconds + restSeconds) + 0.2) * fs);
        std::vector<float> in (total), out;
        std::uint32_t rng = 1;
        for (size_t i = 0; i < total; ++i)
        {
            rng = rng * 1664525u + 1013904223u;
            in[i] = 0.00025f * ((float) (rng >> 8) / 8388608.0f - 1.0f);
            const double t = (double) i / fs - start;
            if (t >= 0.0)
            {
                const double inNote = std::fmod (t, noteSeconds + restSeconds);
                if (inNote < noteSeconds && t < numNotes * (noteSeconds + restSeconds))
                    in[i] += (float) (0.3 * std::exp (-inNote / 0.05) * std::sin (2.0 * 3.14159265358979 * 82.41 * inNote)
                                      * std::min (1.0, (noteSeconds - inNote) / 0.004));
            }
        }

        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        for (size_t pos = 0; pos + block <= total; pos += block)
        {
            for (int ch = 0; ch < 2; ++ch)
                buffer.copyFrom (ch, 0, in.data() + pos, block);
            proc.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + block);
        }
        return out;
    }
}

int gateCheck()
{
    int failures = 0;
    auto check = [&failures] (bool ok, const char* what) { if (! ok) { std::printf ("  FAIL: %s\n", what); ++failures; } };

    const auto gated = render (true), open = render (false);
    double worstRest = -200.0, quietestAttack = 0.0, openRest = 0.0;
    for (int k = 0; k < numNotes; ++k)
    {
        const double note = 0.3 + k * (noteSeconds + restSeconds);
        // the last 50 ms of each rest, and the first 15 ms of each note
        worstRest = std::max (worstRest, rmsDb (gated, note + noteSeconds + restSeconds - 0.05, note + noteSeconds + restSeconds));
        openRest = std::min (openRest, rmsDb (open, note + noteSeconds + restSeconds - 0.05, note + noteSeconds + restSeconds));
        quietestAttack = std::min (quietestAttack, rmsDb (gated, note, note + 0.015) - rmsDb (open, note, note + 0.015));
    }
    std::printf ("gate: rests %.1f dBFS (without the gate %.1f), attacks %+.2f dB against no gate\n", worstRest, openRest, quietestAttack);
    check (openRest > -45.0, "the test signal is not noisy enough to need a gate");
    check (worstRest < -70.0, "the gate leaves the amp's hiss between chugs");
    check (quietestAttack > -1.0, "the gate cuts the pick attack");
    return failures;
}

int bypassCheck()
{
    int failures = 0;
    auto check = [&failures] (bool ok, const char* what) { if (! ok) { std::printf ("  FAIL: %s\n", what); ++failures; } };

    auto makeProcessor = []
    {
        auto proc = std::make_unique<ApexAmpProcessor>();
        proc->setRateAndBufferSizeDetails (fs, block);
        proc->presets.loadPreset (0);
        set (*proc, "dropOn", 1);
        set (*proc, "dropShift", -5);
        set (*proc, "delayOn", 1);
        set (*proc, "delaySync", 0);
        set (*proc, "delayTime", 300);
        set (*proc, "delayFeedback", 85);
        set (*proc, "delayMix", 50);
        proc->prepareToPlay (fs, block);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);   // latency report
        return proc;
    };
    juce::MidiBuffer midi;
    auto runBlocks = [&midi] (ApexAmpProcessor& proc, int blocks, float level, std::vector<float>* out)
    {
        juce::AudioBuffer<float> buffer (2, block);
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    buffer.setSample (ch, i, level * (float) std::sin (2.0 * 3.14159265358979 * 110.0 * (b * block + i) / fs));
            proc.processBlock (buffer, midi);
            if (out != nullptr)
                out->insert (out->end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + block);
        }
    };

    // bypassed with the Drop on: an impulse comes out exactly `latency` samples later
    {
        auto proc = makeProcessor();
        const int latency = proc->getLatencySamples();
        set (*proc, "bypass", 1);
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, block);
        for (int b = 0; b < 8; ++b)
        {
            buffer.clear();
            if (b == 0)
                for (int ch = 0; ch < 2; ++ch)
                    buffer.setSample (ch, 10, 1.0f);
            proc->processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + block);
        }
        int peakAt = 0;
        for (int i = 0; i < (int) out.size(); ++i)
            if (std::abs (out[(size_t) i]) > std::abs (out[(size_t) peakAt]))
                peakAt = i;
        std::printf ("bypass: latency %d, impulse out at +%d samples (%.3f)\n", latency, peakAt - 10, out[(size_t) peakAt]);
        check (latency > 0, "the Drop pedal reports no latency");
        check (peakAt - 10 == latency && std::abs (out[(size_t) peakAt] - 1.0f) < 1.0e-6f, "bypass is not delayed by the reported latency");
    }

    // a note into a long echo, then 20 blocks of silence: once without the
    // bypass, once bypassed for those 20 blocks. Afterwards both must match.
    {
        auto plain = makeProcessor(), bypassed = makeProcessor();
        std::vector<float> a, b;
        runBlocks (*plain, 40, 0.3f, nullptr);
        runBlocks (*plain, 20, 0.0f, nullptr);
        runBlocks (*plain, 200, 0.0f, &a);

        runBlocks (*bypassed, 40, 0.3f, nullptr);
        set (*bypassed, "bypass", 1);
        runBlocks (*bypassed, 20, 0.0f, nullptr);
        set (*bypassed, "bypass", 0);
        runBlocks (*bypassed, 200, 0.0f, &b);

        float worst = 0.0f, tail = 0.0f;
        for (size_t i = 0; i < a.size(); ++i)
        {
            worst = std::max (worst, std::abs (a[i] - b[i]));
            tail = std::max (tail, std::abs (a[i]));
        }
        std::printf ("bypass: echo tail after coming back %.4f, difference from never bypassing %.2e\n", tail, worst);
        check (tail > 1.0e-3f, "the test echo has no tail to compare");
        check (worst < 1.0e-5f, "coming back from the bypass does not continue the tails as if the input had stopped");
    }
    return failures;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    const juce::String which (argc > 1 ? argv[1] : "");
    int failures = 0;
    if (which.isEmpty() || which == "gate")   failures += gateCheck();
    if (which.isEmpty() || which == "bypass") failures += bypassCheck();
    std::printf ("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
