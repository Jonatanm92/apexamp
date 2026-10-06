#pragma once

#include "apex/dsp/LpcFormantCorrector.h"

#include <cstdint>
#include <vector>

namespace apex::dsp
{

/**
 * SplicingShifter  ("Live" engine)
 * ---------------
 * Low-latency time-domain pitch shifter for playing through. The input is
 * written into a ring buffer and read back at the pitch ratio. The read head
 * drifts away from (downshift) or towards (upshift) the write head, so every
 * so often it has to jump. The jump distance is picked by normalised
 * cross-correlation, taking the SHORTEST jump that is close to the best match
 * -- in practice one pitch period -- so the splice is phase-aligned and the
 * latency follows the note being played instead of being sized for the
 * lowest possible one. Splice decisions are made on the mono sum and applied to
 * every channel, so a stereo image never wobbles.
 *
 * Pick attacks are detected at the input; if the read head is lagging when one
 * arrives, it jumps straight to just before the attack, so the attack (what the
 * player feels) always comes out at the minimum latency.
 *
 * Optional zero-latency formant correction (LpcFormantCorrector) keeps the
 * pickup / body resonances in place ("Body").
 */
class SplicingShifter
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;

    void setPitchRatio (float ratio) noexcept;
    void setFormantPreserve (float amount) noexcept { formant.setAmount (amount); }

    /** Delay of the read head at rest, and where every pick attack lands. Between
        attacks the actual delay moves above this value to fit one period of the
        note; this is what should be reported to the host. */
    int getLatencySamples() const noexcept { return nominalDelay; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** Number of splices performed since reset() (for tests / metering). */
    int getSpliceCount() const noexcept { return spliceCount; }

private:
    void maybeStartSplice() noexcept;
    void catchUpToAttack() noexcept;
    float correlation (std::int64_t a, std::int64_t b, int length, int stride) const noexcept;
    float readBuffer (const std::vector<float>& buffer, double position) const noexcept;
    int minimumDelay() const noexcept;

    double sampleRate = 48000.0;
    int numPreparedChannels = 1;
    float ratio = 1.0f;

    std::vector<float> buffers[maxChannels];
    std::vector<float> mono;
    std::int64_t writeIndex = 0;
    std::int64_t mask = 0;

    double delay = 0.0;            // read head delay (samples behind the write head)
    double spliceDelay = 0.0;      // delay of the head being faded in
    bool splicing = false;
    int spliceCounter = 0;
    int spliceCount = 0;

    int crossfadeLength = 288;
    int minJump = 120, maxJump = 1344, lastJump = 480;
    int nominalDelay = 512;
    int decimation = 4;
    int retryCountdown = 0, retryInterval = 128;
    std::vector<float> fade, coarse, sourceWindow;

    // Attack catch-up: when a pick attack arrives while the read head is lagging,
    // jump straight to it so every attack comes out at the minimum latency.
    int attackCushion = 96, refractory = 2400, sinceAttack = 0;
    float fastEnergy = 0.0f, slowEnergy = 0.0f, fastCoeff = 0.0f, slowCoeff = 0.0f, previousMono = 0.0f;
    bool pendingCatchUp = false;

    LpcFormantCorrector formant;
};

} // namespace apex::dsp
