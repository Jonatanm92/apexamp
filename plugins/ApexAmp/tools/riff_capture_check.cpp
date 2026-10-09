// Records a short riff into RiffRecorder the way the engine does, exports it,
// and reads the files back: length and silence trim, kick count and placement
// on the picks, bass latency compensation.
#include <juce_audio_formats/juce_audio_formats.h>
#include "RiffRecorder.h"

#include <cmath>
#include <cstdio>

int main()
{
    const double fs = 48000.0;
    const int block = 256, bassLatency = 384, kickLateness = 200;
    const double onsets[] = { 2.0, 2.25, 2.5, 2.75 };
    int failures = 0;
    auto check = [&failures] (bool ok, const char* what) { if (! ok) { std::printf ("  FAIL: %s\n", what); ++failures; } };

    RiffRecorder recorder;
    recorder.prepare (fs, block);
    recorder.setBassLatency (bassLatency);

    const int total = (int) (4.0 * fs);
    std::vector<float> di ((size_t) total, 0.0f), bass ((size_t) total, 0.0f);
    for (double t0 : onsets)
        for (int i = 0; i < (int) (0.1 * fs); ++i)
        {
            const auto at = (size_t) (t0 * fs) + (size_t) i;
            di[at] = 0.5f * std::exp (-(float) i / 2000.0f) * (float) std::sin (2.0 * 3.14159265 * 82.41 * i / fs);
            if (at + bassLatency < bass.size())
                bass[at + bassLatency] = di[at];   // the bass comes out later, like the pitch engine's
        }

    for (int pos = 0; pos < total; pos += block)
    {
        for (double t0 : onsets)
        {
            const int heard = (int) (t0 * fs) + kickLateness;   // the follower confirms a little late
            if (heard >= pos && heard < pos + block)
                recorder.pushKick (heard - pos, kickLateness, 0.8f);
        }
        recorder.pushBlock (di.data() + pos, bass.data() + pos, block);
    }

    // the last sound is the end of the last note; silence after it changes nothing
    const auto lastSound = recorder.getLastSound();
    check (std::abs ((double) lastSound / fs - (2.75 + 0.1)) < 0.06, "last sound position");
    {
        std::vector<float> silence ((size_t) block, 0.0f);
        recorder.pushBlock (silence.data(), silence.data(), block);
        check (recorder.getLastSound() == lastSound, "silence moved the last sound");
    }

    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("apex_riff_check");
    folder.deleteRecursively();
    const auto files = recorder.exportRiff (folder, 120.0);
    check (files.ok, "export failed");
    std::printf ("riff capture: %.2f s, %d kicks\n", files.seconds, files.numKicks);
    check (std::abs (files.seconds - (2.75 + 0.1 + 0.3 - 2.0 + 0.05)) < 0.12, "riff length / silence trim");
    check (files.numKicks == 4, "kick count");

    // kicks on the picks (the export starts 50 ms before the first note)
    {
        juce::FileInputStream in (files.kicks);
        juce::MidiFile midi;
        check (in.openedOk() && midi.readFrom (in), "MIDI unreadable");
        midi.convertTimestampTicksToSeconds();
        int n = 0;
        double worst = 0.0;
        if (midi.getNumTracks() > 0)
            for (const auto* e : *midi.getTrack (0))
                if (e->message.isNoteOn())
                {
                    const double expected = onsets[n] - (onsets[0] - 0.05);
                    worst = std::max (worst, std::abs (e->message.getTimeStamp() - expected));
                    ++n;
                }
        std::printf ("  MIDI: %d notes, worst placement error %.2f ms\n", n, worst * 1000.0);
        check (n == 4 && worst < 0.001, "kick placement");
    }

    // the bass lines up with the DI again
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        auto first = [&formats] (const juce::File& f)
        {
            std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
            if (r == nullptr) return -1;
            juce::AudioBuffer<float> b (1, (int) r->lengthInSamples);
            r->read (&b, 0, b.getNumSamples(), 0, true, false);
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (std::abs (b.getSample (0, i)) > 0.01f) return i;
            return -1;
        };
        const int a = first (files.di), b = first (files.bass);
        std::printf ("  first note: DI at %d, bass at %d samples\n", a, b);
        check (a >= 0 && std::abs (a - b) <= 2, "bass not aligned with the DI");
    }

    folder.deleteRecursively();
    std::printf ("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
