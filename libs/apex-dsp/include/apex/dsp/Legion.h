#pragma once

#include "apex/dsp/Filters.h"
#include "apex/dsp/PitchShifter.h"

#include <array>
#include <cstdint>
#include <vector>

namespace apex::dsp
{

/**
 * The Legion: the band that plays along with a riff.
 *
 * RiffFollower finds the picked notes in a DI and decides which of them a kick
 * drum follows; KickSynth plays a modern metal kick; BassFollower doubles the
 * riff an octave down with a split clean / driven bass voicing. Everything is
 * real-time safe after prepare() and identical for any host block size.
 */

//==============================================================================
/** Finds pick attacks in a DI. In all-notes mode every attack is a hit. In
    chug mode an attack becomes a hit once the low band confirms the note has
    real weight there (the low strings, where a djent kick lives); a low note
    cannot be told from a high one before part of its first period has passed,
    so these hits come a few ms after the pick. `lateness` says how many, so a
    recorder can place the hit exactly on the pick. */
class RiffFollower
{
public:
    enum class Mode { chugs, allNotes };
    struct Hit { int offset; float velocity; int lateness; };
    static constexpr int maxHitsPerBlock = 64;

    void prepare (double sampleRate);
    void reset() noexcept;

    void setMode (Mode m) noexcept { mode = m; }
    /** 0..1: how small an attack still counts. */
    void setSensitivity (float s) noexcept { sensitivity = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }

    /** Analyses a block; the hits found are available until the next call. */
    int process (const float* di, int numSamples) noexcept;
    const Hit& getHit (int index) const noexcept { return hits[(size_t) index]; }

private:
    struct Detector
    {
        EnvelopeFollower fast, slow;
        bool armed = true;
        void reset() noexcept { fast.reset(); slow.reset(); armed = true; }
    };

    double sampleRate = 48000.0;
    Mode mode = Mode::chugs;
    float sensitivity = 0.5f;

    Biquad highpass, low1, low2;
    Detector full, low;
    float loudest = 1.0e-3f, lowLoudest = 1.0e-4f, loudestDecay = 0.0f;
    int sinceHit = 1 << 30, minInterval = 1920;
    // chug mode: an attack waiting for the low band to confirm it
    bool pending = false;
    int pendingAge = 0, confirmWindow = 672, pendingRise = 0;
    // samples since the full-band envelope last stopped falling: where the pick began
    int rising = 0, maxBackdate = 720;
    float previousFast = 0.0f;
    float pendingLowStart = 0.0f, pendingVelocity = 0.0f;
    std::array<Hit, maxHitsPerBlock> hits {};
};

//==============================================================================
/** A modern metal kick: pitch-swept sine body, beater click, gentle drive.
    Two voices alternate so fast doubles never cut each other off. */
class KickSynth
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    /** 0 = deep and round, 1 = higher, tighter, more click. */
    void setTone (float t) noexcept { tone = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

    void trigger (float velocity) noexcept;
    float next() noexcept;
    bool isActive() const noexcept { return voices[0].amp > 1.0e-5f || voices[1].amp > 1.0e-5f; }

private:
    struct Voice
    {
        double phase = 0.0;
        float t = 0.0f, amp = 0.0f, velocity = 0.0f, release = 1.0f;
        bool releasing = false;
        Biquad clickFilter;
        std::uint32_t noise = 1u;
    };

    double sampleRate = 48000.0;
    float tone = 0.5f;
    std::array<Voice, 2> voices;
    int nextVoice = 0;
};

//==============================================================================
/** Doubles a DI an octave (plus any drop) down and voices it like a modern
    metal bass: a clean sub band under a driven, low-passed mid band. */
class BassFollower
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setSemitones (float s) noexcept { semitones = s; }
    void setGrit (float g) noexcept { grit = g < 0.0f ? 0.0f : (g > 1.0f ? 1.0f : g); }

    /** Writes the bass for `di` into `out` (may be the same buffer). */
    void process (const float* di, float* out, int numSamples) noexcept;
    int getLatencySamples() const noexcept { return shifter.getLatencySamples(); }

private:
    double sampleRate = 48000.0;
    float semitones = -12.0f, grit = 0.4f;
    PitchShifter shifter;
    Biquad subLow1, subLow2, midHigh1, midHigh2, midLow, inputLow;
    OnePole gritSmoother;
};

} // namespace apex::dsp
