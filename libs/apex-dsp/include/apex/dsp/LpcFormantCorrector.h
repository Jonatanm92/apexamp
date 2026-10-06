#pragma once

#include <array>
#include <vector>

namespace apex::dsp
{

/**
 * LpcFormantCorrector
 * -------------------
 * Zero-latency spectral-envelope correction for the Live engine. A resampling
 * pitch shifter drags the formants (pickup / body resonances) along with the
 * pitch, which is what makes a dropped guitar sound small and muffled.
 *
 * Every hop it fits an all-pole (LPC) envelope to both the unshifted source and
 * the shifted output, then filters the output through
 *
 *     whiten with A_out(z)  ->  re-colour with 1 / A_target(z)
 *
 * where A_target morphs from A_out (amount 0: untouched) to A_source (amount 1:
 * formants held where the real guitar has them). Both filters are lattices
 * whose reflection coefficients are interpolated per sample, which keeps them
 * stable while they move.
 */
class LpcFormantCorrector
{
public:
    static constexpr int order = 24;
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;

    /** 0 = leave the shifted envelope alone, 1 = restore the source envelope. */
    void setAmount (float newAmount) noexcept { amount = newAmount; }
    bool isActive() const noexcept { return amount > 0.0f || ! atRest; }

    /** Length of the source window setSource() expects. */
    int getAnalysisLength() const noexcept { return analysisLength; }

    /** True when the next process() call refits the envelopes; the caller must
        then provide the source window first. */
    bool wantsSource() const noexcept { return hopCounter + 1 >= hopLength && isActive(); }

    /** The unshifted content (mono, oldest first, getAnalysisLength() samples)
        ending where the shifter is currently reading: the envelope to restore. */
    void setSource (const float* oldestFirst) noexcept;

    /** Filters one frame (frame[ch]) of shifted output in place. */
    void process (float* frame, int numChannels) noexcept;

private:
    using Coefficients = std::array<float, order>;

    void analyse() noexcept;
    float levinson (const float* oldestFirst, Coefficients& k) noexcept;

    int analysisLength = 1024, hopLength = 256, writePos = 0, hopCounter = 0;
    float amount = 0.0f;
    bool atRest = true;
    bool previousAmountWasZero = true;

    std::vector<float> window, lagWindow, source, outputHistory, scratch;
    std::array<double, order + 1> autocorrelation {};

    Coefficients whiten {}, whitenStep {}, colour {}, colourStep {};
    float gain = 1.0f, gainStep = 0.0f;

    // Safety: the corrected signal may never get more than +6 dB louder than
    // the uncorrected one (envelope mismatch at note changes, DC, noise).
    float dryEnergy = 0.0f, wetEnergy = 0.0f, energyCoeff = 0.01f;

    struct LatticeState { std::array<float, order> whitenB {}, colourB {}; };
    std::array<LatticeState, maxChannels> state {};
};

} // namespace apex::dsp
