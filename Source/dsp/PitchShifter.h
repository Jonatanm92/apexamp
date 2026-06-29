#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace apex
{
/**
    Monophonic granular pitch shifter — the "Whammy".

    Classic two-tap overlap-add (granular delay) pitch shifter: the input is written
    to a ring buffer and read back by two pointers, offset half a grain apart, whose
    delay ramps over time. Hann windows crossfade between them so there are no clicks
    at the grain wrap. Shifting the read rate shifts the pitch. Sits in front of the
    amp like a Whammy pedal; the Shift control is fully automatable for dive-bombs
    and octave runs, and Mix lets you blend a dry+shifted harmony.

    Off by default (Mix is irrelevant when disabled) so it never colours the tone.
*/
class PitchShifter
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        buffer.assign ((size_t) bufSize, 0.0f);
        grain = (float) (fs * 0.050); // 50 ms grains
        reset();
        setParams (false, 0.0f, 1.0f);
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        phase = 0.0f;
    }

    /** on/off, shift in semitones (-24..+24), mix 0..1 (1 = fully shifted). */
    void setParams (bool on, float semitones, float mixAmount) noexcept
    {
        enabled = on;
        ratio   = std::pow (2.0f, semitones / 12.0f);
        mix     = std::clamp (mixAmount, 0.0f, 1.0f);
    }

    inline float processSample (float x) noexcept
    {
        if (! enabled)
            return x;

        buffer[(size_t) (writePos & mask)] = x;

        // Two read taps, half a grain apart, with a slowly ramping delay.
        const float step = (1.0f - ratio) / grain;
        phase += step;
        phase -= std::floor (phase);                 // wrap to [0,1)
        float phase2 = phase + 0.5f;
        phase2 -= std::floor (phase2);

        const float d1 = phase  * grain;
        const float d2 = phase2 * grain;
        const float s1 = readFrac ((float) writePos - d1);
        const float s2 = readFrac ((float) writePos - d2);

        // Hann crossfade windows (sum ~ constant power).
        const float w1 = 0.5f * (1.0f - std::cos (2.0f * 3.14159265f * phase));
        const float w2 = 0.5f * (1.0f - std::cos (2.0f * 3.14159265f * phase2));
        const float shifted = s1 * w1 + s2 * w2;

        ++writePos;
        return x * (1.0f - mix) + shifted * mix;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    inline float readFrac (float idx) const noexcept
    {
        while (idx < 0.0f) idx += (float) bufSize;
        const int i0 = (int) idx & mask;
        const int i1 = (i0 + 1) & mask;
        const float f = idx - std::floor (idx);
        return buffer[(size_t) i0] + f * (buffer[(size_t) i1] - buffer[(size_t) i0]);
    }

    static constexpr int bufSize = 1 << 15; // 32768
    static constexpr int mask    = bufSize - 1;

    double fs = 44100.0;
    bool   enabled = false;
    float  ratio = 1.0f, mix = 1.0f, grain = 2205.0f, phase = 0.0f;
    int    writePos = 0;
    std::vector<float> buffer;
};
} // namespace apex
