#pragma once

#include "Biquad.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace apex
{
/**
    Instant double-tracker / stereo widener — a standout feature for rhythm metal.

    Real "double-tracked" guitars = two separate takes hard-panned L/R, which gives
    that huge, wide wall of sound. Recording two tight takes is hard; this fakes a
    convincing double from ONE mono performance by sending decorrelated versions to
    L and R: each side gets a different short modulated delay (so the two "takes"
    drift slightly, like a human re-take) plus a small tone offset.

    Width = 0 leaves the signal centred mono (no change). As Width increases, the
    sides decorrelate into a wide, doubled image.
*/
class Doubler
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        const int len = (int) (fs * 0.06) + 4; // 60 ms max
        bufL.assign ((size_t) len, 0.0f);
        bufR.assign ((size_t) len, 0.0f);
        size = len;

        baseL = (float) (fs * 0.014);  // 14 ms
        baseR = (float) (fs * 0.024);  // 24 ms
        depth = (float) (fs * 0.0022); // ~2.2 ms detune sweep
        incL  = (float) (0.31 / fs);   // slow LFOs at slightly different rates
        incR  = (float) (0.41 / fs);

        toneL = Biquad::makeHighShelf (fs, 3000.0, 0.7, 1.5);
        toneR = Biquad::makeHighShelf (fs, 3000.0, 0.7, -1.5);
        reset();
    }

    void reset() noexcept
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        writePos = 0; phaseL = 0.0f; phaseR = 0.25f;
        toneL.reset(); toneR.reset();
    }

    void setWidth (float w) noexcept { width = std::clamp (w, 0.0f, 1.0f); }

    /** Stereo in place. On entry L and R carry the (identical) mono amp signal. */
    void process (float* L, float* R, int numSamples) noexcept
    {
        if (width <= 0.0001f)
            return; // stay centred mono

        for (int i = 0; i < numSamples; ++i)
        {
            const float mono = 0.5f * (L[i] + R[i]);

            bufL[(size_t) writePos] = mono;
            bufR[(size_t) writePos] = mono;

            phaseL += incL; if (phaseL >= 1.0f) phaseL -= 1.0f;
            phaseR += incR; if (phaseR >= 1.0f) phaseR -= 1.0f;
            const float dL = baseL + depth * std::sin (2.0f * 3.14159265f * phaseL);
            const float dR = baseR + depth * std::sin (2.0f * 3.14159265f * phaseR);

            float wetL = toneL.processSample (readFrac (bufL, dL));
            float wetR = toneR.processSample (readFrac (bufR, dR));

            L[i] = mono * (1.0f - width) + wetL * width;
            R[i] = mono * (1.0f - width) + wetR * width;

            if (++writePos >= size) writePos = 0;
        }
    }

private:
    inline float readFrac (const std::vector<float>& buf, float delaySamples) const noexcept
    {
        float rp = (float) writePos - delaySamples;
        while (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float frac = rp - (float) i0;
        return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

    double fs = 44100.0;
    int  size = 0, writePos = 0;
    float baseL = 0, baseR = 0, depth = 0, incL = 0, incR = 0;
    float phaseL = 0, phaseR = 0.25f, width = 0.0f;
    std::vector<float> bufL, bufR;
    Biquad toneL, toneR;
};
} // namespace apex
