#pragma once

#include "apex/dsp/SpectralShifter.h"
#include "apex/dsp/SplicingShifter.h"

#include <vector>

namespace apex::dsp
{

enum class PitchMode
{
    live   = 0,   // low latency, for playing through (SplicingShifter)
    studio = 1    // polyphonic, highest quality, latency compensated (SpectralShifter)
};

/**
 * PitchShifter
 * ------------
 * The pitch engine the plugins talk to: picks the Live or Studio engine, maps
 * "Body" onto each engine's formant control and blends a latency-aligned dry
 * signal. Pure C++, no JUCE, real-time safe after prepare().
 */
class PitchShifter
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    /** Switching mode resets the newly selected engine and changes the latency. */
    void setMode (PitchMode newMode) noexcept;
    PitchMode getMode() const noexcept { return mode; }

    /** Total shift in semitones (fractional values give cents). */
    void setSemitones (float semitones) noexcept;

    /** 1 = keep the guitar's body / pickup resonances in place (a real drop
        tuning), 0 = let them move with the pitch (classic shifter sound). */
    void setBody (float amount) noexcept;

    /** Dry / wet, 0..1. */
    void setMix (float newMix) noexcept { mixTarget = newMix; }

    int getLatencySamples() const noexcept { return getLatencySamples (mode); }
    int getLatencySamples (PitchMode m) const noexcept;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    void updateEngines() noexcept;

    SpectralShifter studio;
    SplicingShifter live;
    PitchMode mode = PitchMode::live;

    int numPreparedChannels = 1;
    float ratio = 1.0f, body = 1.0f;
    float mix = 1.0f, mixTarget = 1.0f, mixStep = 0.0f;

    std::vector<float> dryDelay[maxChannels];
    int dryWrite = 0, drySize = 1;
};

} // namespace apex::dsp
