#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

/**
 * RiffRecorder
 * ------------
 * Always listening: keeps the last ~32 seconds of the DI, the Legion's bass and
 * its kick hits, whether or not the host is recording. The audio thread only
 * writes into preallocated rings; the editor exports the last riff on the
 * message thread as files you drag into the DAW:
 *
 *   riff_DI.wav    the undropped DI after the input trim (re-amp it later)
 *   riff_bass.wav  the bass, moved back by its pitch engine's latency
 *   riff_kick.mid  a GM kick (note 36) on every hit, placed on the pick
 *
 * Silence before and after the riff is trimmed off.
 */
class RiffRecorder
{
public:
    static constexpr double ringSeconds = 32.0, maxExportSeconds = 30.0;

    void prepare (double sampleRate, int maxBlockSize)
    {
        rate = sampleRate;
        capacity = (std::int64_t) std::ceil (ringSeconds * sampleRate) + 2 * maxBlockSize;
        diRing.assign ((size_t) capacity, 0.0f);
        bassRing.assign ((size_t) capacity, 0.0f);
        written.store (0);
        lastSound.store (-1);
        kickCount.store (0);
        blockStart = 0;
    }

    void setBassLatency (int samples) noexcept { bassLatency = samples; }

    /** Audio thread, once per block before pushBlock: a kick `lateness` samples
        before `offset` within the coming block. */
    void pushKick (int offset, int lateness, float velocity) noexcept
    {
        const auto index = kickCount.load (std::memory_order_relaxed);
        kicks[(size_t) (index % kicks.size())] = { blockStart + offset - lateness, velocity };
        kickCount.store (index + 1, std::memory_order_release);
    }

    /** Audio thread. bass may be null (the Legion is off). */
    void pushBlock (const float* di, const float* bass, int numSamples) noexcept
    {
        auto w = written.load (std::memory_order_relaxed);
        std::int64_t sound = -1;
        for (int i = 0; i < numSamples; ++i, ++w)
        {
            const auto at = (size_t) (w % capacity);
            diRing[at] = di[i];
            bassRing[at] = bass != nullptr ? bass[i] : 0.0f;
            if (std::abs (di[i]) > silenceThreshold)
                sound = w;
        }
        if (sound >= 0)
            lastSound.store (sound, std::memory_order_relaxed);
        written.store (w, std::memory_order_release);
        blockStart = w;
    }

    struct Files { juce::File di, bass, kicks; double seconds = 0.0; int numKicks = 0; bool ok = false; };

    /** Where the DI last had sound (a sample count, -1 = never): while it stays
        the same, nothing new was played and the last export is still the riff. */
    std::int64_t getLastSound() const noexcept { return lastSound.load (std::memory_order_relaxed); }

    /** Message thread: writes the last riff (up to 30 s) into `folder`. */
    Files exportRiff (const juce::File& folder, double bpm) const
    {
        Files files;
        const auto end = written.load (std::memory_order_acquire);
        if (end <= 0 || rate <= 0.0)
            return files;

        const auto span = std::min<std::int64_t> (end, (std::int64_t) (maxExportSeconds * rate));
        auto start = end - span;
        std::vector<float> di ((size_t) span), bass ((size_t) span);
        for (std::int64_t i = 0; i < span; ++i)
        {
            const auto at = (size_t) ((start + i) % capacity);
            di[(size_t) i] = diRing[at];
        }
        for (std::int64_t i = 0; i < span; ++i)
        {
            const auto src = start + i + bassLatency;   // the bass came out this much later
            bass[(size_t) i] = src < end ? bassRing[(size_t) (src % capacity)] : 0.0f;
        }

        // trim to the riff: from just before the first note to just after the last
        const float threshold = silenceThreshold;
        std::int64_t first = -1, last = -1;
        for (std::int64_t i = 0; i < span; ++i)
            if (std::abs (di[(size_t) i]) > threshold) { if (first < 0) first = i; last = i; }
        if (first < 0)
            return files;
        const auto from = std::max<std::int64_t> (0, first - (std::int64_t) (0.05 * rate));
        const auto to = std::min<std::int64_t> (span, last + (std::int64_t) (0.3 * rate));
        const auto length = (int) (to - from);
        const auto origin = start + from;

        folder.createDirectory();
        files.di = folder.getChildFile ("riff_DI.wav");
        files.bass = folder.getChildFile ("riff_bass.wav");
        files.kicks = folder.getChildFile ("riff_kick.mid");
        files.seconds = (double) length / rate;

        const bool wroteDi = writeWav (files.di, di.data() + from, length);
        const bool wroteBass = writeWav (files.bass, bass.data() + from, length);

        // kicks inside the window, on a MIDI clock at the host tempo
        juce::MidiMessageSequence sequence;
        const double ticksPerSecond = 960.0 * juce::jlimit (20.0, 400.0, bpm) / 60.0;
        sequence.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / juce::jlimit (20.0, 400.0, bpm))), 0.0);
        const auto total = kickCount.load (std::memory_order_acquire);
        const auto oldest = total > kicks.size() ? total - (std::uint32_t) kicks.size() : 0u;
        for (auto k = oldest; k < total; ++k)
        {
            const auto& e = kicks[(size_t) (k % kicks.size())];
            if (e.time < origin || e.time >= origin + length)
                continue;
            const double tick = (double) (e.time - origin) / rate * ticksPerSecond;
            const auto velocity = (juce::uint8) juce::jlimit (1, 127, (int) std::lround (e.velocity * 127.0f));
            sequence.addEvent (juce::MidiMessage::noteOn (10, 36, velocity), tick);
            sequence.addEvent (juce::MidiMessage::noteOff (10, 36), tick + 60.0);
            ++files.numKicks;
        }
        sequence.updateMatchedPairs();
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (960);
        midi.addTrack (sequence);
        files.kicks.deleteFile();
        bool wroteMidi = false;
        if (juce::FileOutputStream out (files.kicks); out.openedOk())
            wroteMidi = midi.writeTo (out);

        files.ok = wroteDi && wroteBass && wroteMidi;
        return files;
    }

private:
    struct KickEvent { std::int64_t time; float velocity; };
    static constexpr float silenceThreshold = 0.003f;

    bool writeWav (const juce::File& file, const float* data, int numSamples) const
    {
        file.deleteFile();
        auto stream = std::make_unique<juce::FileOutputStream> (file);
        if (! stream->openedOk())
            return false;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), rate, 1, 24, {}, 0));
        if (writer == nullptr)
            return false;
        stream.release();   // the writer owns it now
        const float* channels[1] = { data };
        return writer->writeFromFloatArrays (channels, 1, numSamples);
    }

    double rate = 48000.0;
    std::int64_t capacity = 1, blockStart = 0;
    int bassLatency = 0;
    std::vector<float> diRing, bassRing;
    std::atomic<std::int64_t> written { 0 }, lastSound { -1 };
    std::array<KickEvent, 2048> kicks {};
    std::atomic<std::uint32_t> kickCount { 0 };
};
