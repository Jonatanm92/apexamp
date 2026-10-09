#pragma once

#include "apex/dsp/Fft.h"

#include <vector>

namespace apex::dsp
{

/**
 * SpectralShifter  ("Studio" engine)
 * ---------------
 * Polyphonic phase-vocoder pitch shifter that shifts spectral peaks rather than
 * resampling (Laroche & Dolson, "New phase-vocoder techniques for pitch-shifting,
 * harmonizing and other exotic effects", 1999):
 *
 *   - every spectral peak and its region of influence is moved to the target
 *     frequency as a block, with identity phase locking inside the region, so
 *     partials stay coherent instead of sounding "phasey";
 *   - peak phases are propagated by tracking each peak from the previous frame;
 *   - on a detected pick attack the synthesis phases are reset to the analysis
 *     phases, which keeps palm mutes and attacks tight;
 *   - the spectral envelope (cepstrum) is measured every frame so the formants
 *     (pickup / body resonances) can be held in place or moved with the pitch.
 *
 * The latency is fixed (window - hop) and should be reported to the host.
 * All memory is allocated in prepare(); process() is real-time safe.
 */
class SpectralShifter
{
public:
    static constexpr int maxChannels = 2;

    /** Allocates all buffers. The FFT size scales with the sample rate so the
        analysis window stays ~85 ms (4096 points at 44.1 / 48 kHz). */
    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;

    /** Pitch ratio (2^(semitones/12)). Applied from the next analysis frame. */
    void setPitchRatio (float ratio) noexcept { pitchRatio = ratio; }

    /** Scale applied to the spectral envelope. 1 keeps the formants where they
        are; setting it equal to the pitch ratio moves them with the pitch. */
    void setFormantRatio (float ratio) noexcept { formantRatio = ratio; }

    /** A sample entering the FIFO comes out after one full window. */
    int getLatencySamples() const noexcept { return fftSize; }
    int getFftSize() const noexcept { return fftSize; }

    /** Processes numChannels (<= the prepared count) in place. */
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    struct Channel
    {
        std::vector<float> inFifo, outFifo, outAccum;
        std::vector<float> magnitude, phase, frequency, lastPhase, prevMagnitude;
        std::vector<float> synthPhase, prevSynthPhase;
        std::vector<int>   peakBins, peakTargets, prevPeakBins, prevPeakTargets;
        int numPeaks = 0, numPrevPeaks = 0;
        std::vector<Fft::Complex> spectrum, output;
    };

    void processFrame (int numChannels) noexcept;
    void analyse (Channel&) noexcept;
    float attackEnergyRatio (Channel&) noexcept;
    void computeEnvelopeGains (const Channel&) noexcept;
    void synthesise (Channel&, bool attack) noexcept;
    int findPreviousPeak (const Channel&, int bin) const noexcept;

    Fft fft;
    int fftOrder = 12, fftSize = 4096, hop = 1024, halfSize = 2048;
    int rover = 0;
    int numPreparedChannels = 0;
    double sampleRate = 48000.0;
    float olaScale = 1.0f;
    int attackLowBin = 1;
    int framesSinceAttack = 1000;
    float attackFloor = 0.0f;
    float energyAverage = 0.0f;
    int lifterLength = 48;

    float pitchRatio = 1.0f, formantRatio = 1.0f;
    float framePitchRatio = 1.0f;       // ratio latched for the current frame

    std::vector<float> window;
    std::vector<float> envelope, envelopeGain;
    std::vector<Fft::Complex> cepstrum;
    Channel channels[maxChannels];
};

} // namespace apex::dsp
