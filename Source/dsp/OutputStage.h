#pragma once

#include "Biquad.h"
#include <cmath>
#include <algorithm>

namespace apex
{
/**
    Final output stage: Punch (transient emphasis) + Loud (makeup + peak limiter).

    Crucially this adds loudness/punch WITHOUT any waveshaping/clipping, so it
    introduces no aliasing and can run safely at host rate (an earlier version
    used host-rate tanh clipping, which aliased and sounded "digital/synth").

      - Punch: a fast-vs-slow envelope detector finds note onsets and briefly
        lifts the gain (pure multiplication, no harmonics) so the first hit slams.
      - Loud: a clean makeup gain pushes the level, and a transparent feed-forward
        peak limiter (gain *reduction* only, no clipping) tames the resulting peaks
        for a dense, loud, in-your-face level — like a record, not a fuzz.

    Both default near-neutral so the existing tone is untouched until dialed in.
*/
class OutputStage
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        fastEnv.prepare (fs); fastEnv.setTimes (0.3f, 12.0f);
        slowEnv.prepare (fs); slowEnv.setTimes (25.0f, 160.0f);
        // limiter ballistics
        limAtk = 1.0f - std::exp (-1.0f / (float (fs) * 0.0008f)); // ~0.8 ms attack
        limRel = 1.0f - std::exp (-1.0f / (float (fs) * 0.120f));  // ~120 ms release
        reset();
    }

    void reset() noexcept
    {
        fastEnv.reset(); slowEnv.reset();
        punchSm = 0.0f;
        limGain = 1.0f;
    }

    /** punch: 0..1 attack emphasis. loudDb: makeup into the limiter (0..~12 dB). */
    void setParams (float punch01, float loudDb) noexcept
    {
        punchAmt = std::clamp (punch01, 0.0f, 1.0f);
        makeup   = std::pow (10.0f, loudDb / 20.0f);
    }

    inline float processSample (float x) noexcept
    {
        // --- Punch: transient-driven gain (no waveshaping) ---
        const float f = fastEnv.processSample (x);
        const float s = slowEnv.processSample (x) + 1.0e-6f;
        float trans = std::clamp ((f - s) / s, 0.0f, 1.0f);
        punchSm += 0.05f * (trans * punchAmt - punchSm);
        const float punchGain = 1.0f + punchSm * 2.0f;     // up to ~+6 dB on attacks

        x *= punchGain * makeup;

        // --- Loud: transparent peak limiter (gain reduction only) ---
        const float a = std::fabs (x);
        const float desired = (a > ceiling) ? (ceiling / a) : 1.0f;
        const float c = (desired < limGain) ? limAtk : limRel;
        limGain += c * (desired - limGain);
        return x * limGain;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    double fs = 44100.0;
    float  punchAmt = 0.3f;
    float  makeup   = 1.0f;
    float  punchSm  = 0.0f;

    static constexpr float ceiling = 0.98f;
    float  limAtk = 0.0f, limRel = 0.0f, limGain = 1.0f;

    EnvelopeFollower fastEnv, slowEnv;
};
} // namespace apex
