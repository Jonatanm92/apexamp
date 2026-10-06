// Renders a WAV file through the apex-dsp pitch engine.
//
//   apex_pitch_render in.wav out.wav [--semitones -7] [--mode live|studio]
//                     [--body 1.0] [--mix 1.0] [--trim]
//
// --trim removes the engine latency from the start of the output so it lines
// up with the input in a DAW.

#include "apex/dsp/PitchShifter.h"
#include "WavIO.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf ("usage: %s in.wav out.wav [--semitones N] [--mode live|studio] [--body 0..1] [--mix 0..1] [--trim]\n", argv[0]);
        return 2;
    }

    float semitones = -7.0f, body = 1.0f, mix = 1.0f;
    auto mode = apex::dsp::PitchMode::studio;
    bool trim = false;

    for (int i = 3; i < argc; ++i)
    {
        const std::string a = argv[i];
        const char* next = i + 1 < argc ? argv[i + 1] : "";
        if (a == "--semitones")  { semitones = (float) std::atof (next); ++i; }
        else if (a == "--body")  { body = (float) std::atof (next); ++i; }
        else if (a == "--mix")   { mix = (float) std::atof (next); ++i; }
        else if (a == "--mode")  { mode = std::strcmp (next, "live") == 0 ? apex::dsp::PitchMode::live : apex::dsp::PitchMode::studio; ++i; }
        else if (a == "--trim")  { trim = true; }
        else { std::printf ("unknown option %s\n", a.c_str()); return 2; }
    }

    apex::test::Audio audio;
    if (! apex::test::readWav (argv[1], audio))
    {
        std::printf ("could not read %s\n", argv[1]);
        return 1;
    }
    if (audio.numChannels() > 2)
        audio.channels.resize (2);

    apex::dsp::PitchShifter shifter;
    shifter.prepare (audio.sampleRate, 512, audio.numChannels());
    shifter.setMode (mode);
    shifter.setSemitones (semitones);
    shifter.setBody (body);
    shifter.setMix (mix);
    shifter.reset();

    const int latency = shifter.getLatencySamples();
    for (auto& ch : audio.channels)
        ch.resize (ch.size() + (size_t) latency, 0.0f);

    const int total = audio.numSamples();
    for (int pos = 0; pos < total; pos += 512)
    {
        float* ptrs[2] {};
        for (int ch = 0; ch < audio.numChannels(); ++ch)
            ptrs[ch] = audio.channels[(size_t) ch].data() + pos;
        shifter.process (ptrs, audio.numChannels(), std::min (512, total - pos));
    }

    if (trim)
        for (auto& ch : audio.channels)
            ch.erase (ch.begin(), ch.begin() + latency);

    if (! apex::test::writeWav (argv[2], audio))
    {
        std::printf ("could not write %s\n", argv[2]);
        return 1;
    }

    std::printf ("%s -> %s  (%+.2f st, %s, body %.2f, mix %.2f, latency %d samples)\n", argv[1], argv[2],
                 semitones, mode == apex::dsp::PitchMode::live ? "live" : "studio", body, mix, latency);
    return 0;
}
