#pragma once

#include "Biquad.h"
#include <cmath>
#include <algorithm>

namespace apex
{
/**
    Final output stage — the "make it hit like a record" block.

    Commercial amp sims feel loud and punchy on the very first note because of
    what happens AFTER the cab: a dense soft-clip maximiser that raises perceived
    loudness, plus transient emphasis so the pick attack pokes out above that
    dense floor. This recreates both:

      1. Soft-clip maximiser: drive into tanh raises RMS / density and glues the
         tone together, while the soft knee keeps peaks musical (no harsh digital
         clipping). This is most of the "louder / bigger" impression.

      2. Transient punch: a fast-vs-slow envelope detector finds note onsets and
         briefly lifts the gain on attacks, so the first hit slams before the
         maximiser density takes over on the sustain.

    A gentle final ceiling keeps the output bounded so attacks never clip nastily.
*/
class OutputStage
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        fastEnv.prepare (fs); fastEnv.setTimes (0.3f, 10.0f);
        slowEnv.prepare (fs); slowEnv.setTimes (25.0f, 150.0f);
        reset();
    }

    void reset() noexcept
    {
        fastEnv.reset(); slowEnv.reset();
        punchSm = 0.0f;
    }

    /** punch: 0..1 attack emphasis. driveDb: maximiser drive (loudness/density). */
    void setParams (float punch01, float driveDb) noexcept
    {
        punchAmt = std::clamp (punch01, 0.0f, 1.0f);
        drive    = std::pow (10.0f, driveDb / 20.0f);
    }

    inline float processSample (float x) noexcept
    {
        // 1) density / loudness
        float dense = std::tanh (x * drive);

        // 2) transient punch on top of the dense floor
        const float f = fastEnv.processSample (x);
        const float s = slowEnv.processSample (x) + 1.0e-6f;
        float trans = std::clamp ((f - s) / s, 0.0f, 1.0f);
        punchSm += 0.05f * (trans * punchAmt - punchSm);
        const float g = 1.0f + punchSm * 1.8f;   // up to ~+5 dB on hard attacks

        float y = dense * g;

        // 3) hard-bounded ceiling (tanh saturates below 1.0) so attacks never clip
        y = std::tanh (y);
        return y;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    double fs = 44100.0;
    float  punchAmt = 0.6f;
    float  drive    = 2.0f;   // +6 dB default into the maximiser
    float  punchSm  = 0.0f;
    EnvelopeFollower fastEnv, slowEnv;
};
} // namespace apex
