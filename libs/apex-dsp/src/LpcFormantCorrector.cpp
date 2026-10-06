#include "apex/dsp/LpcFormantCorrector.h"

#include <algorithm>
#include <cmath>

namespace apex::dsp
{

void LpcFormantCorrector::prepare (double sampleRate, int numChannels)
{
    (void) numChannels;

    // ~21 ms analysis window, refreshed every ~5 ms.
    analysisLength = 1;
    while ((double) analysisLength < sampleRate * 0.021)
        analysisLength <<= 1;
    hopLength = analysisLength / 4;
    attackRampStep = (float) hopLength / (float) (0.025 * sampleRate);

    constexpr double twoPi = 6.283185307179586476925286766559;
    window.resize ((size_t) analysisLength);
    for (int n = 0; n < analysisLength; ++n)
        window[(size_t) n] = (float) (0.5 - 0.5 * std::cos (twoPi * (n + 0.5) / analysisLength));

    // Gaussian lag window (~150 Hz bandwidth) so the all-pole fit follows the
    // broad envelope instead of locking onto individual low harmonics.
    peakRelease     = (float) std::exp (-1.0 / (0.01 * sampleRate));            // 10 ms
    limiterRecovery = (float) (1.0 - std::exp (-1.0 / (0.005 * sampleRate)));   // 5 ms

    lagWindow.resize (order + 1);
    for (int i = 0; i <= order; ++i)
    {
        const double x = twoPi * 150.0 * i / sampleRate;
        lagWindow[(size_t) i] = (float) std::exp (-0.5 * x * x);
    }

    source.assign ((size_t) analysisLength, 0.0f);
    outputHistory.assign ((size_t) analysisLength, 0.0f);
    scratch.assign ((size_t) analysisLength, 0.0f);

    reset();
}

void LpcFormantCorrector::reset() noexcept
{
    std::fill (source.begin(), source.end(), 0.0f);
    std::fill (outputHistory.begin(), outputHistory.end(), 0.0f);
    whiten.fill (0.0f);  whitenStep.fill (0.0f);
    colour.fill (0.0f);  colourStep.fill (0.0f);
    gain = 1.0f; gainStep = 0.0f;
    dryPeak = wetPeak = 0.0f;
    limiterGain = 1.0f;
    attackRamp = 1.0f;
    for (auto& s : state) { s.whitenB.fill (0.0f); s.colourB.fill (0.0f); }
    writePos = hopCounter = 0;
    atRest = true;
    previousAmountWasZero = true;
}

void LpcFormantCorrector::setSource (const float* oldestFirst) noexcept
{
    std::copy (oldestFirst, oldestFirst + analysisLength, source.begin());
}

float LpcFormantCorrector::levinson (const float* oldestFirst, Coefficients& k) noexcept
{
    for (int n = 0; n < analysisLength; ++n)
        scratch[(size_t) n] = oldestFirst[n] * window[(size_t) n];

    for (int lag = 0; lag <= order; ++lag)
    {
        double acc = 0.0;
        for (int n = lag; n < analysisLength; ++n)
            acc += (double) scratch[(size_t) n] * scratch[(size_t) (n - lag)];
        autocorrelation[(size_t) lag] = acc * lagWindow[(size_t) lag];
    }

    k.fill (0.0f);
    if (autocorrelation[0] < 1.0e-9)
        return 1.0f;                      // silence: flat envelope

    autocorrelation[0] *= 1.0001;         // white-noise correction (conditioning)

    std::array<double, order + 1> a {}, tmp {};
    a[0] = 1.0;
    double error = autocorrelation[0];

    for (int m = 1; m <= order; ++m)
    {
        double acc = autocorrelation[(size_t) m];
        for (int i = 1; i < m; ++i)
            acc += a[(size_t) i] * autocorrelation[(size_t) (m - i)];

        const double km = std::clamp (-acc / error, -0.995, 0.995);
        for (int i = 1; i < m; ++i)
            tmp[(size_t) i] = a[(size_t) i] + km * a[(size_t) (m - i)];
        for (int i = 1; i < m; ++i)
            a[(size_t) i] = tmp[(size_t) i];
        a[(size_t) m] = km;

        error *= 1.0 - km * km;
        k[(size_t) (m - 1)] = (float) km;
    }

    return (float) (error / autocorrelation[0]);     // normalised prediction error
}

void LpcFormantCorrector::notifyAttack() noexcept
{
    if (! isActive())
        return;

    // Re-colouring with the whitening envelope is the identity.
    colour = whiten;
    colourStep = whitenStep;
    gain = 1.0f;
    gainStep = 0.0f;
    attackRamp = 0.0f;
    hopCounter = hopLength - 1;    // refit on the next sample
}

void LpcFormantCorrector::analyse() noexcept
{
    // Unroll the circular output history (oldest first) into the source's
    // spare copy slot: scratch is overwritten inside levinson().
    Coefficients sourceK {}, shifted {};
    levinson (source.data(), sourceK);

    for (int n = 0; n < analysisLength; ++n)
        source[(size_t) n] = outputHistory[(size_t) ((writePos + n) % analysisLength)];
    const float shiftedError = levinson (source.data(), shifted);

    const float effective = amount * attackRamp;
    attackRamp = std::min (1.0f, attackRamp + attackRampStep);

    Coefficients target {};
    float targetError = 1.0f;
    for (int m = 0; m < order; ++m)
    {
        target[(size_t) m] = shifted[(size_t) m] + effective * (sourceK[(size_t) m] - shifted[(size_t) m]);
        targetError *= 1.0f - target[(size_t) m] * target[(size_t) m];
    }

    // Keeps the corrected output at the level of the shifted signal.
    const float targetGain = std::clamp (std::sqrt (targetError / std::max (shiftedError, 1.0e-6f)),
                                         1.0e-3f, 4.0f);

    const float inv = 1.0f / (float) hopLength;
    for (int m = 0; m < order; ++m)
    {
        whitenStep[(size_t) m] = (shifted[(size_t) m] - whiten[(size_t) m]) * inv;
        colourStep[(size_t) m] = (target[(size_t) m] - colour[(size_t) m]) * inv;
    }
    gainStep = (targetGain - gain) * inv;

    // Only rest once a whole hop has faded back to the identity.
    atRest = amount <= 0.0f && previousAmountWasZero;
    previousAmountWasZero = amount <= 0.0f;
}

void LpcFormantCorrector::process (float* frame, int numChannels) noexcept
{
    numChannels = std::min (numChannels, maxChannels);

    float shiftedMono = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        shiftedMono += frame[ch];
    shiftedMono /= (float) std::max (1, numChannels);

    outputHistory[(size_t) writePos] = shiftedMono;
    if (++writePos == analysisLength)
        writePos = 0;

    if (++hopCounter >= hopLength)
    {
        hopCounter = 0;
        if (isActive())
        {
            analyse();
        }
        else
        {
            whitenStep.fill (0.0f);
            colourStep.fill (0.0f);
            gainStep = 0.0f;
        }
    }

    for (int m = 0; m < order; ++m)
    {
        whiten[(size_t) m] += whitenStep[(size_t) m];
        colour[(size_t) m] += colourStep[(size_t) m];
    }
    gain += gainStep;

    if (atRest && amount <= 0.0f)
        return;   // whitening and re-colouring with the same envelope is the identity

    float corrected = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& s = state[(size_t) ch];

        // Whitening lattice, A_out(z).
        float f = frame[ch];
        float bPrev = f;                         // b_0(n)
        for (int m = 0; m < order; ++m)
        {
            const float km = whiten[(size_t) m];
            const float bDelayed = s.whitenB[(size_t) m];   // b_m(n-1)
            const float fNext = f + km * bDelayed;
            const float bNext = bDelayed + km * f;
            s.whitenB[(size_t) m] = bPrev;
            bPrev = bNext;
            f = fNext;
        }

        // Re-colouring lattice, 1 / A_target(z).
        float g = f;
        for (int m = order - 1; m >= 0; --m)
        {
            const float km = colour[(size_t) m];
            g -= km * s.colourB[(size_t) m];
            if (m + 1 < order)
                s.colourB[(size_t) m + 1] = s.colourB[(size_t) m] + km * g;
        }
        s.colourB[0] = g;

        frame[ch] = g * gain;
        corrected += frame[ch];
    }

    corrected /= (float) std::max (1, numChannels);
    dryPeak = std::max (std::abs (shiftedMono), dryPeak * peakRelease);
    wetPeak = std::max (std::abs (corrected), wetPeak * peakRelease);

    // Instant attack, 5 ms recovery.
    const float allowed = std::min (1.0f, 1.5f * dryPeak / std::max (wetPeak, 1.0e-9f));
    limiterGain = allowed < limiterGain ? allowed : limiterGain + limiterRecovery * (allowed - limiterGain);

    for (int ch = 0; ch < numChannels; ++ch)
        frame[ch] *= limiterGain;
}

} // namespace apex::dsp
