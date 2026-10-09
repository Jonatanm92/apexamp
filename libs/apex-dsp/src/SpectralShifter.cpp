#include "apex/dsp/SpectralShifter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace apex::dsp
{

namespace
{
    constexpr float pi    = 3.14159265358979323846f;
    constexpr float twoPi = 6.28318530717958647692f;
    constexpr int   overlap = 4;

    // Reported attack when the energy above ~1.5 kHz jumps by this factor (+6 dB)
    // within one hop.
    constexpr float attackRiseRatio = 4.0f;

    // Formant correction is limited to +-24 dB so noise-floor bins never explode.
    const float maxLogGain = 24.0f / 20.0f * std::log (10.0f);

    inline float wrapPhase (float p) noexcept
    {
        return p - twoPi * std::floor ((p + pi) / twoPi);
    }
}

void SpectralShifter::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    numPreparedChannels = std::clamp (numChannels, 1, maxChannels);

    // ~85 ms analysis window: long enough to resolve the harmonics of a low
    // F# / E on an 8-string.
    fftOrder = 10;
    while ((double) (1 << fftOrder) < sampleRate * 0.08)
        ++fftOrder;

    fftSize  = 1 << fftOrder;
    hop      = fftSize / overlap;
    halfSize = fftSize / 2;
    fft.prepare (fftOrder);

    window.resize ((size_t) fftSize);
    double sumSquares = 0.0;
    for (int n = 0; n < fftSize; ++n)
    {
        const float w = 0.5f - 0.5f * std::cos (twoPi * (float) n / (float) fftSize);
        window[(size_t) n] = w;
        sumSquares += (double) w * w;
    }
    olaScale = (float) ((double) hop / sumSquares);

    attackLowBin = std::max (1, (int) std::ceil (1500.0 * fftSize / sampleRate));
    attackFloor  = (float) ((double) fftSize * fftSize / 16.0 * 1.0e-7);   // ~ -70 dBFS
    lifterLength = std::max (8, (int) std::lround (0.001 * sampleRate));   // 1 ms quefrency

    const auto bins = (size_t) (halfSize + 1);
    envelope.assign (bins, 0.0f);
    envelopeGain.assign (bins, 1.0f);
    cepstrum.assign ((size_t) fftSize, {});

    for (auto& c : channels)
    {
        c.inFifo.assign ((size_t) fftSize, 0.0f);
        c.outFifo.assign ((size_t) fftSize, 0.0f);
        c.outAccum.assign ((size_t) (2 * fftSize), 0.0f);

        for (auto* v : { &c.magnitude, &c.phase, &c.frequency, &c.lastPhase,
                         &c.prevMagnitude, &c.synthPhase, &c.prevSynthPhase })
            v->assign (bins, 0.0f);

        for (auto* v : { &c.peakBins, &c.peakTargets, &c.prevPeakBins, &c.prevPeakTargets })
            v->assign (bins, 0);

        c.spectrum.assign ((size_t) fftSize, {});
        c.output.assign ((size_t) fftSize, {});
    }

    reset();
}

void SpectralShifter::reset() noexcept
{
    for (auto& c : channels)
    {
        for (auto* v : { &c.inFifo, &c.outFifo, &c.outAccum, &c.magnitude, &c.phase,
                         &c.frequency, &c.lastPhase, &c.prevMagnitude, &c.synthPhase,
                         &c.prevSynthPhase })
            std::fill (v->begin(), v->end(), 0.0f);

        c.numPeaks = c.numPrevPeaks = 0;
    }

    rover = fftSize - hop;
    framesSinceAttack = 1000;
}

void SpectralShifter::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, numPreparedChannels);
    const int fifoStart = fftSize - hop;   // where new input lands in the FIFO

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& c = channels[ch];
            c.inFifo[(size_t) rover] = io[ch][i];
            io[ch][i] = c.outFifo[(size_t) (rover - fifoStart)];
        }

        if (++rover >= fftSize)
        {
            rover = fifoStart;
            processFrame (numChannels);
        }
    }
}

void SpectralShifter::processFrame (int numChannels) noexcept
{
    framePitchRatio = pitchRatio;

    float rise = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        analyse (channels[ch]);
        rise = std::max (rise, attackEnergyRatio (channels[ch]));
    }

    // Stereo-linked attack decision, so both sides reset on the same frame.
    const bool attack = rise > attackRiseRatio && framesSinceAttack >= 2;
    framesSinceAttack = attack ? 0 : framesSinceAttack + 1;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& c = channels[ch];
        synthesise (c, attack);

        // Hermitian spectrum -> real output frame.
        c.output[0] = { c.output[0].real(), 0.0f };
        c.output[(size_t) halfSize] = { c.output[(size_t) halfSize].real(), 0.0f };
        for (int k = 1; k < halfSize; ++k)
            c.output[(size_t) (fftSize - k)] = std::conj (c.output[(size_t) k]);

        fft.inverse (c.output.data());

        for (int n = 0; n < fftSize; ++n)
            c.outAccum[(size_t) n] += c.output[(size_t) n].real() * window[(size_t) n] * olaScale;

        std::memcpy (c.outFifo.data(), c.outAccum.data(), sizeof (float) * (size_t) hop);
        std::memmove (c.outAccum.data(), c.outAccum.data() + hop, sizeof (float) * (size_t) fftSize);
        std::fill (c.outAccum.begin() + fftSize, c.outAccum.end(), 0.0f);
        std::memmove (c.inFifo.data(), c.inFifo.data() + hop, sizeof (float) * (size_t) (fftSize - hop));
    }
}

void SpectralShifter::analyse (Channel& c) noexcept
{
    for (int n = 0; n < fftSize; ++n)
        c.spectrum[(size_t) n] = { c.inFifo[(size_t) n] * window[(size_t) n], 0.0f };

    fft.forward (c.spectrum.data());

    const float expected = twoPi * (float) hop / (float) fftSize;   // phase advance per bin per hop

    for (int k = 0; k <= halfSize; ++k)
    {
        const auto& x = c.spectrum[(size_t) k];
        const float ph = std::atan2 (x.imag(), x.real());
        const float dp = wrapPhase (ph - c.lastPhase[(size_t) k] - (float) k * expected);

        c.magnitude[(size_t) k] = std::sqrt (x.real() * x.real() + x.imag() * x.imag());
        c.phase[(size_t) k]     = ph;
        c.lastPhase[(size_t) k] = ph;
        c.frequency[(size_t) k] = ((float) k * expected + dp) / (float) hop;   // rad / sample
    }
}

float SpectralShifter::attackEnergyRatio (Channel& c) noexcept
{
    float now = 0.0f, before = 0.0f;
    for (int k = attackLowBin; k <= halfSize; ++k)
    {
        now    += c.magnitude[(size_t) k] * c.magnitude[(size_t) k];
        before += c.prevMagnitude[(size_t) k] * c.prevMagnitude[(size_t) k];
    }
    std::copy (c.magnitude.begin(), c.magnitude.end(), c.prevMagnitude.begin());

    return now < attackFloor ? 0.0f : now / (before + attackFloor);
}

void SpectralShifter::computeEnvelopeGains (const Channel& c) noexcept
{
    const float r = framePitchRatio;
    const float f = formantRatio;

    if (std::abs (f / r - 1.0f) < 1.0e-4f)
    {
        std::fill (envelopeGain.begin(), envelopeGain.end(), 1.0f);
        return;
    }

    // Real cepstrum of the log-magnitude spectrum, low-quefrency lifter, back
    // to a smooth log envelope.
    for (int k = 0; k < fftSize; ++k)
    {
        const float m = c.magnitude[(size_t) (k <= halfSize ? k : fftSize - k)];
        cepstrum[(size_t) k] = { std::log (std::max (m, 1.0e-9f)), 0.0f };
    }

    fft.inverse (cepstrum.data());
    for (int k = lifterLength + 1; k < fftSize - lifterLength; ++k)
        cepstrum[(size_t) k] = {};
    fft.forward (cepstrum.data());

    for (int k = 0; k <= halfSize; ++k)
        envelope[(size_t) k] = cepstrum[(size_t) k].real();

    auto logEnvelopeAt = [this] (float bin) noexcept
    {
        if (bin >= (float) halfSize)
            return envelope[(size_t) halfSize];
        const int i = (int) bin;
        const float frac = bin - (float) i;
        return envelope[(size_t) i] + frac * (envelope[(size_t) i + 1] - envelope[(size_t) i]);
    };

    // Output bin k holds content moved from k / r, which carries the envelope
    // value E(k / r). The wanted envelope is E(k / f).
    for (int k = 0; k <= halfSize; ++k)
    {
        const float g = logEnvelopeAt ((float) k / f) - logEnvelopeAt ((float) k / r);
        envelopeGain[(size_t) k] = std::exp (std::clamp (g, -maxLogGain, maxLogGain));
    }
}

int SpectralShifter::findPreviousPeak (const Channel& c, int bin) const noexcept
{
    if (c.numPrevPeaks == 0)
        return -1;

    const auto begin = c.prevPeakBins.begin();
    const auto end   = begin + c.numPrevPeaks;
    const auto it    = std::lower_bound (begin, end, bin);

    int best = -1, bestDistance = std::max (2, bin / 32);
    if (it != end && *it - bin <= bestDistance)
    {
        best = (int) (it - begin);
        bestDistance = *it - bin;
    }
    if (it != begin && bin - *(it - 1) <= bestDistance)
        best = (int) (it - begin) - 1;

    return best;
}

void SpectralShifter::synthesise (Channel& c, bool attack) noexcept
{
    std::fill (c.output.begin(), c.output.end(), Fft::Complex {});

    const float r = framePitchRatio;

    // Unity ratio with untouched formants: pass the analysis frame straight
    // through (perfect reconstruction, so a 0-semitone setting nulls).
    if (r == 1.0f && formantRatio == 1.0f)
    {
        std::copy (c.spectrum.begin(), c.spectrum.begin() + halfSize + 1, c.output.begin());
        c.numPeaks = c.numPrevPeaks = 0;
        return;
    }

    computeEnvelopeGains (c);

    // 1. Spectral peaks (local maxima within 80 dB of the loudest bin).
    float loudest = 0.0f;
    for (int k = 1; k < halfSize; ++k)
        loudest = std::max (loudest, c.magnitude[(size_t) k]);

    const float threshold = std::max (loudest * 1.0e-4f, 1.0e-9f);
    c.numPeaks = 0;
    for (int k = 1; k < halfSize; ++k)
    {
        const float m = c.magnitude[(size_t) k];
        if (m > threshold && m > c.magnitude[(size_t) k - 1] && m >= c.magnitude[(size_t) k + 1])
            c.peakBins[(size_t) c.numPeaks++] = k;
    }

    // 2. Move each peak's region of influence as one block.
    const float binsPerRadian = (float) fftSize / twoPi;
    const float radiansPerBin = twoPi / (float) fftSize;
    const float frameCentre   = 0.5f * (float) fftSize;
    int lo = 0;

    for (int i = 0; i < c.numPeaks; ++i)
    {
        const int p = c.peakBins[(size_t) i];

        int hi = halfSize;
        if (i + 1 < c.numPeaks)
        {
            const int next = c.peakBins[(size_t) i + 1];
            hi = p;
            for (int k = p + 1; k < next; ++k)
                if (c.magnitude[(size_t) k] < c.magnitude[(size_t) hi])
                    hi = k;
        }

        // True frequency of the peak (kept within +-1 bin of where it was found).
        const float w  = std::clamp (c.frequency[(size_t) p],
                                     (float) (p - 1) * radiansPerBin,
                                     (float) (p + 1) * radiansPerBin);
        const float wt = w * r;
        const int q = (int) std::lround (wt * binsPerRadian);
        c.peakTargets[(size_t) i] = -1;

        if (q >= 1 && q < halfSize)
        {
            // Continue the partial's phase from the previous frame when it can be
            // tracked; otherwise (new note, or a pick attack) align its phase with
            // the input at the frame centre so the attack keeps its shape.
            float peakPhase;
            const int prev = attack ? -1 : findPreviousPeak (c, p);
            const int prevTarget = prev >= 0 ? c.prevPeakTargets[(size_t) prev] : -1;

            if (prevTarget >= 0)
                peakPhase = c.prevSynthPhase[(size_t) prevTarget] + (float) hop * wt;
            else
                peakPhase = c.phase[(size_t) p] + (w - wt) * frameCentre;

            peakPhase = wrapPhase (peakPhase);
            c.synthPhase[(size_t) q] = peakPhase;
            c.peakTargets[(size_t) i] = q;

            const int shift = q - p;
            const float peakAnalysisPhase = c.phase[(size_t) p];

            for (int k = lo; k <= hi; ++k)
            {
                const int kk = k + shift;
                if (kk < 0 || kk > halfSize)
                    continue;

                const float ph = peakPhase + (c.phase[(size_t) k] - peakAnalysisPhase);
                const float a  = c.magnitude[(size_t) k] * envelopeGain[(size_t) kk];
                c.output[(size_t) kk] += Fft::Complex { a * std::cos (ph), a * std::sin (ph) };
            }
        }

        lo = hi + 1;
    }

    std::swap (c.peakBins, c.prevPeakBins);
    std::swap (c.peakTargets, c.prevPeakTargets);
    std::swap (c.synthPhase, c.prevSynthPhase);
    c.numPrevPeaks = c.numPeaks;
}

} // namespace apex::dsp
