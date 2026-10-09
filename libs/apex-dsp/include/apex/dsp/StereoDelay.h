#pragma once

#include "apex/dsp/Filters.h"

#include <cstdint>
#include <vector>

namespace apex::dsp
{

/**
 * StereoDelay
 * -----------
 * A delay for a guitar rig: ping-pong or stereo repeats that get darker and
 * lose their sub-lows on every pass (so a 7- or 8-string riff never turns to
 * mud under the repeats), a gentle tape-style saturation in the loop, slight
 * wow for width, and ducking that keeps the repeats down while you play and
 * lets them bloom in the gaps.
 *
 * The dry signal passes through untouched; the repeats are added on top. When
 * switched off, the input stops feeding the delay but the repeats already in
 * it ring out (spillover). Time changes crossfade between read heads instead
 * of pitch-bending.
 */
class StereoDelay
{
public:
    static constexpr double maxDelaySeconds = 2.5;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setEnabled (bool shouldBeOn) noexcept { enabled = shouldBeOn; }
    void setTime (double milliseconds) noexcept;
    void setFeedback (float amount01) noexcept;   // 0..1 -> 0..95 % per repeat
    void setMix (float level01) noexcept       { mix = level01; }
    void setTone (float tone01) noexcept;         // dark .. bright repeats
    void setPingPong (bool shouldPingPong) noexcept { pingPong = shouldPingPong; }
    void setDuck (float amount01) noexcept     { duck = amount01; }

    /** Adds the repeats to the buffers. right may be null (mono). */
    void process (float* left, float* right, int numSamples) noexcept;

    /** False once switched off and the repeats have died away. */
    bool isRinging() const noexcept { return ! asleep; }

    double getDelaySamples() const noexcept { return headDelay[active]; }

private:
    float readLine (const std::vector<float>& line, double delaySamples) const noexcept;
    void designTone() noexcept;

    double sampleRate = 48000.0;
    std::vector<float> lines[2];
    std::int64_t writeIndex = 0, mask = 0;

    bool enabled = false, pingPong = true, asleep = true;
    float mix = 0.3f, feedback = 0.35f, duck = 0.0f, tone = 0.5f, designedTone = -1.0f;

    // Two read heads; a time change fades from the active one to the other.
    double headDelay[2] { 0.0, 0.0 };
    double timeMs = 375.0, targetDelay = 4.0;
    int active = 0, fadePosition = 0, fadeLength = 1;
    bool fading = false;

    OnePole send, mixSmoother, feedbackSmoother;
    OnePole toneLowpass[2], loopHighpass[2];
    EnvelopeFollower duckEnvelope;
    SlowOscillator wow;
    float wowDepth = 0.0f;
    int quietSamples = 0;
};

} // namespace apex::dsp
