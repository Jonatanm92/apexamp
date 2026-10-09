// Renders the ApexAmp demo clips through the real plugin processor.
//   apexamp_demo <out-folder> [--set id=value ...]
// A thall / djent riff is played twice (two humanised takes of a synthetic
// 8-string DI in F# standard, hard left and right, as metal guitars are
// recorded), calibrated with Auto Input like a user would, and rendered:
//   01_di.wav        the dry DI (take A)
//   02_rhythm.wav    Thallbyssal, double-tracked
//   03_legion.wav    the same with The Legion: kick on every chug, bass an octave down
//   04_drop.wav      the riff played in E standard, taken down to F# by the Drop pedal, with The Legion
// Stereo, 48 kHz, 24-bit. --set overrides a parameter in every clip.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GuitarDI.h"

#include <cstdio>
#include <map>

using apex::demo::Note;

namespace
{
    constexpr double fs = 48000.0;
    constexpr int block = 256;
    constexpr double bpm = 135.0, step = 60.0 / bpm / 4.0;   // a sixteenth

    // 8-string, F# standard
    constexpr double F1s = 46.25, G1 = 49.00, C2s = 69.30, F2s = 92.50, G2 = 98.00, C3 = 130.81, C3s = 138.59;

    /** The riff: a 13-step cell over 4/4 (it comes round on a different beat
        every time), a power-chord stab at the head of each cell, palm-muted
        chugs and rests; the second half answers with a dissonant stab, b2
        chugs and 32nd-note bursts; it ends on a ringing cluster. */
    std::vector<Note> riff (std::uint32_t seed, double transpose)
    {
        apex::demo::Rng rng (seed);
        std::vector<Note> notes;
        const double k = std::pow (2.0, transpose / 12.0);
        auto t = [&rng] (double s) { return 0.25 + s * step + 0.0025 * rng.gauss(); };
        auto add = [&] (double hz, double start, double length, float pm, float vel)
        {
            notes.push_back ({ hz * k, start, length, pm, std::clamp (vel + 0.04f * rng.bipolar(), 0.3f, 1.0f) });
        };
        auto stab = [&] (double s, bool dissonant, double steps)
        {
            const double at = t (s);
            const double spread = 0.004;   // a downstroke reaches the strings one after another
            if (dissonant)
            {
                add (F1s, at, steps * step * 0.95, 0.2f, 1.0f);
                add (G2, at + spread, steps * step * 0.95, 0.2f, 0.85f);
                add (C3, at + 2 * spread, steps * step * 0.95, 0.2f, 0.8f);
            }
            else
            {
                add (F1s, at, steps * step * 0.95, 0.25f, 1.0f);
                add (C2s, at + spread, steps * step * 0.95, 0.25f, 0.9f);
                add (F2s, at + 2 * spread, steps * step * 0.95, 0.25f, 0.85f);
            }
        };
        auto chug = [&] (double s, double hz, double len = 0.8) { add (hz, t (s), len * step, 1.0f, 0.86f); };

        // cell: S . c c . c . . c c . c .   (13 sixteenths)
        const int cell[13] = { 2, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0 };
        for (int s = 0; s < 128; ++s)
        {
            const int c = cell[s % 13], index = s / 13;
            const bool second = s >= 64;
            if (c == 2)
                stab (s, second && index % 2 == 1, 2.0);
            else if (c == 1)
            {
                const double hz = second && (s % 13) >= 8 ? G1 : F1s;
                if (second && (s % 13) == 11)
                {
                    chug (s, hz, 0.45);           // a 32nd-note burst
                    chug (s + 0.5, hz, 0.45);
                }
                else
                    chug (s, hz);
            }
        }
        // the end: a ringing cluster, let go after a bar
        const double end = t (128.0);
        int i = 0;
        for (double hz : { F1s, C2s, F2s, G2, C3s })
            add (hz, end + 0.006 * i++, 24 * step, 0.0f, 0.95f);
        return notes;
    }

    struct Settings { std::map<juce::String, float> params; };

    void setParam (ApexAmpProcessor& p, const juce::String& id, float value)
    {
        if (auto* param = p.apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
        else
            std::printf ("  (no parameter %s)\n", id.toRawUTF8());
    }

    void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    struct Result { std::vector<float> out; float trimDb = 0.0f; int latency = 0; };

    /** Runs a DI through a fresh processor. With calibrate, runs Auto Input
        on it first (the way a user starts) and keeps the trim it finds. */
    Result run (const std::vector<float>& di, const Settings& s, bool calibrate, float trimDb)
    {
        ApexAmpProcessor proc;
        proc.setRateAndBufferSizeDetails (fs, block);
        proc.prepareToPlay (fs, block);
        proc.presets.loadPreset (0);   // Init / Flat
        for (const auto& [id, v] : s.params)
            setParam (proc, id, v);
        pump (50);

        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        Result r;
        if (calibrate)
        {
            proc.startAutoInput();
            for (size_t pos = 0; pos + block <= di.size(); pos += block)
            {
                for (int ch = 0; ch < 2; ++ch)
                    buffer.copyFrom (ch, 0, di.data() + pos, block);
                proc.processBlock (buffer, midi);
                if ((pos / block) % 32 == 0)
                    pump (1);
            }
            pump (300);
            trimDb = proc.getAutoInputTrim();
        }
        setParam (proc, "inputTrim", trimDb);
        r.trimDb = trimDb;
        proc.prepareToPlay (fs, block);   // a clean start for the take
        pump (200);
        r.latency = proc.getLatencySamples();

        const size_t total = di.size() + (size_t) r.latency + block;
        std::vector<float> out;
        out.reserve (total);
        for (size_t pos = 0; pos < total; pos += block)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                buffer.clear (ch, 0, block);
                if (pos < di.size())
                    buffer.copyFrom (ch, 0, di.data() + pos, (int) std::min<size_t> (block, di.size() - pos));
            }
            proc.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + block);
        }
        r.out.assign (out.begin() + r.latency, out.begin() + (std::ptrdiff_t) (r.latency + di.size()));
        std::printf ("  run: trim %+.1f dB, latency %d\n", r.trimDb, r.latency);
        return r;
    }

    void writeStereo (const juce::File& file, const std::vector<float>& left, const std::vector<float>& right, float peakDb = -1.0f)
    {
        float peak = 1.0e-9f;
        for (size_t i = 0; i < left.size(); ++i)
            peak = std::max ({ peak, std::abs (left[i]), std::abs (right[i]) });
        const float g = juce::Decibels::decibelsToGain (peakDb) / peak;
        juce::AudioBuffer<float> b (2, (int) left.size());
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            b.setSample (0, i, left[(size_t) i] * g);
            b.setSample (1, i, right[(size_t) i] * g);
        }
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (file), fs, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        std::printf ("wrote %s (%.1f s, peak gain %+.1f dB)\n", file.getFileName().toRawUTF8(), left.size() / fs,
                     juce::Decibels::gainToDecibels (g));
    }

    std::vector<float> mixInto (std::vector<float> a, const std::vector<float>& b, float gb)
    {
        for (size_t i = 0; i < a.size() && i < b.size(); ++i)
            a[i] += gb * b[i];
        return a;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    if (argc < 2)
    {
        std::printf ("usage: apexamp_demo <out-folder> [--set id=value ...]\n");
        return 2;
    }
    const bool probe = juce::String (argv[1]) == "--probe";
    const juce::File folder = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    if (! probe)
        folder.createDirectory();

    // the tone: a modern djent rhythm sound
    Settings tone;
    tone.params = { { "rigMode", 0 }, { "rig", 0 }, { "ir", 1 }, { "cabMix", 100 },
                    { "tight", 130 }, { "presence", 3 }, { "lowCut", 85 }, { "depth", 1.5f }, { "fizz", 35 },
                    { "bass", 0 }, { "mid", 0.5f }, { "treble", 2 },
                    { "gateOn", 1 }, { "gate", -55 }, { "gateHold", 30 },
                    { "boostOn", 1 }, { "boostDrive", 10 }, { "boostTone", 60 }, { "boostLevel", 6 },
                    { "shapeOn", 1 }, { "chug", 35 } };
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--set")
        {
            const juce::String kv (argv[++i]);
            tone.params[kv.upToFirstOccurrenceOf ("=", false, false)] = kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        }

    if (probe)
    {
        // levels through the processor (absolute dBFS per 100 ms): silence, then
        // the riff, or a DI from a file given as --di path
        std::vector<float> input;
        for (int i = 2; i + 1 < argc; ++i)
            if (juce::String (argv[i]) == "--di")
            {
                juce::AudioFormatManager formats;
                formats.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (argv[i + 1])));
                juce::AudioBuffer<float> b (1, (int) reader->lengthInSamples);
                reader->read (&b, 0, b.getNumSamples(), 0, true, false);
                input.assign (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
            }
        if (input.empty())
        {
            auto di = apex::demo::renderDI (riff (11, 0.0), fs, 2.5, 11, 0.11f);
            input.assign ((size_t) (1.0 * fs), 0.0f);
            input.insert (input.end(), di.begin(), di.end());
        }
        float trim = 5.0f, scale = 1.0f;
        for (int i = 2; i + 1 < argc; ++i)
        {
            if (juce::String (argv[i]) == "--trim") trim = juce::String (argv[i + 1]).getFloatValue();
            if (juce::String (argv[i]) == "--scale") scale = juce::String (argv[i + 1]).getFloatValue();
        }
        for (auto& v : input)
            v *= scale;
        const auto r = run (input, tone, false, trim);
        for (size_t at = 0; at + 4800 <= r.out.size(); at += 4800)
        {
            double in = 0.0, out = 0.0;
            for (size_t i = at; i < at + 4800; ++i) { in += input[i] * input[i]; out += r.out[i] * r.out[i]; }
            std::printf ("%4.1f s  in %6.1f  out %6.1f dBFS\n", at / fs, 10.0 * std::log10 (in / 4800 + 1e-20), 10.0 * std::log10 (out / 4800 + 1e-20));
        }
        return 0;
    }

    Settings legion = tone;
    legion.params.insert_or_assign ("legionOn", 1.0f);
    legion.params.insert_or_assign ("kickMode", 0.0f);
    legion.params.insert_or_assign ("kickLevel", 70.0f);
    legion.params.insert_or_assign ("kickTone", 55.0f);
    legion.params.insert_or_assign ("bassLevel", 60.0f);
    legion.params.insert_or_assign ("bassGrit", 45.0f);

    const double seconds = 0.25 + 152 * step + 1.5;
    const auto takeA = apex::demo::renderDI (riff (11, 0.0), fs, seconds, 11, 0.11f);
    const auto takeB = apex::demo::renderDI (riff (29, 0.0), fs, seconds, 29, 0.13f);

    // 1: the DI
    writeStereo (folder.getChildFile ("01_di.wav"), takeA, takeA, -3.0f);

    // 2: double-tracked rhythm (Auto Input sets the trim once, as a user would)
    const auto calibration = run (takeA, tone, true, 0.0f);
    std::printf ("Auto Input trim: %+.1f dB, latency %d samples\n", calibration.trimDb, calibration.latency);
    const auto left = run (takeA, tone, false, calibration.trimDb).out;
    const auto right = run (takeB, tone, false, calibration.trimDb).out;
    writeStereo (folder.getChildFile ("02_rhythm.wav"), left, right);

    // 3: with The Legion. Its kick and bass follow take A and sit in the
    // middle: take A with the Legion minus take A without it is the Legion alone.
    const auto leftLegion = run (takeA, legion, false, calibration.trimDb).out;
    std::vector<float> band (left.size());
    for (size_t i = 0; i < band.size(); ++i)
        band[i] = leftLegion[i] - left[i];
    writeStereo (folder.getChildFile ("03_legion.wav"), mixInto (left, band, 0.71f), mixInto (right, band, 0.71f));
    writeStereo (folder.getChildFile ("03b_legion_alone.wav"), band, band, -3.0f);

    // 4: the riff on a 6-string in E standard (ten semitones up), dropped to F# by the Drop pedal
    Settings drop = legion;
    drop.params.insert_or_assign ("dropOn", 1.0f);
    drop.params.insert_or_assign ("dropShift", -10.0f);
    drop.params.insert_or_assign ("dropBody", 100.0f);
    drop.params.insert_or_assign ("dropSub", 0.0f);
    Settings dropDry = drop;
    dropDry.params.insert_or_assign ("legionOn", 0.0f);
    const auto standardA = apex::demo::renderDI (riff (11, 10.0), fs, seconds, 11, 0.11f);
    const auto standardB = apex::demo::renderDI (riff (29, 10.0), fs, seconds, 29, 0.13f);
    const auto dropCal = run (standardA, dropDry, true, 0.0f);
    const auto dl = run (standardA, dropDry, false, dropCal.trimDb).out;
    const auto dr = run (standardB, dropDry, false, dropCal.trimDb).out;
    const auto dlLegion = run (standardA, drop, false, dropCal.trimDb).out;
    std::vector<float> dropBand (dl.size());
    for (size_t i = 0; i < dropBand.size(); ++i)
        dropBand[i] = dlLegion[i] - dl[i];
    writeStereo (folder.getChildFile ("04_drop.wav"), mixInto (dl, dropBand, 0.71f), mixInto (dr, dropBand, 0.71f));
    writeStereo (folder.getChildFile ("04b_drop_di.wav"), standardA, standardA, -3.0f);
    return 0;
}
