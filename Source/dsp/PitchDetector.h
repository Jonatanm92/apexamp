#pragma once

#include <vector>
#include <cmath>

namespace apex
{
/**
    Lightweight monophonic pitch detector for the tuner.

    Collects the dry input into a ring buffer and, periodically, runs a normalised
    autocorrelation (with a simple parabolic peak refinement) to estimate the
    fundamental frequency of a single played note. Designed for guitar range
    (~60-1300 Hz). It is intentionally simple and cheap; it feeds a UI readout,
    not the audio path, so latency/precision are non-critical.

    Returns 0 Hz when the signal is too quiet or no stable pitch is found.
*/
class PitchDetector
{
public:
    void prepare (double sampleRate, int /*blockSize*/) noexcept
    {
        fs = sampleRate;
        bufferSize = (int) (fs * 0.06); // 60 ms window (enough for low E)
        if (bufferSize < 1024) bufferSize = 1024;
        buffer.assign ((size_t) bufferSize, 0.0f);
        writePos = 0;
        hopCounter = 0;
        hopSize = (int) (fs * 0.02); // analyse ~ every 20 ms
        currentFreq = 0.0f;
    }

    /** Push one input sample (pre-gain DI). Thread: audio. */
    inline void pushSample (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        if (++writePos >= bufferSize) writePos = 0;

        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;
            analyse();
        }
    }

    /** Latest detected frequency in Hz (0 = none). Thread: audio writes, UI reads. */
    float getFrequency() const noexcept { return currentFreq; }

private:
    void analyse() noexcept
    {
        const int n = bufferSize;

        // Linearise the ring buffer into a contiguous, mean-removed window.
        work.resize ((size_t) n);
        double mean = 0.0, rms = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float s = buffer[(size_t) ((writePos + i) % n)];
            work[(size_t) i] = s;
            mean += s;
        }
        mean /= n;
        for (int i = 0; i < n; ++i)
        {
            work[(size_t) i] -= (float) mean;
            rms += (double) work[(size_t) i] * work[(size_t) i];
        }
        rms = std::sqrt (rms / n);

        if (rms < 0.0008) { currentFreq = 0.0f; return; } // too quiet

        const int minLag = (int) (fs / 1300.0); // highest pitch ~1300 Hz
        const int maxLag = (int) (fs / 60.0);   // lowest pitch ~60 Hz

        // Normalised autocorrelation; find the first strong peak after the dip.
        float bestValue = 0.0f;
        int   bestLag = -1;
        float prev = 0.0f;
        bool  rising = false;

        const float zeroLag = autocorr (0);
        if (zeroLag <= 0.0f) { currentFreq = 0.0f; return; }

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            const float v = autocorr (lag) / zeroLag;

            if (v > prev) rising = true;
            if (rising && v > 0.6f && v > bestValue)
            {
                bestValue = v;
                bestLag = lag;
            }
            prev = v;
        }

        if (bestLag <= 0 || bestValue < 0.6f) { currentFreq = 0.0f; return; }

        // Parabolic interpolation around the peak for sub-sample accuracy.
        const float y0 = autocorr (bestLag - 1) / zeroLag;
        const float y1 = autocorr (bestLag)     / zeroLag;
        const float y2 = autocorr (bestLag + 1) / zeroLag;
        const float denom = (y0 - 2.0f * y1 + y2);
        float delta = 0.0f;
        if (std::fabs (denom) > 1.0e-9f)
            delta = 0.5f * (y0 - y2) / denom;

        const float refinedLag = (float) bestLag + delta;
        if (refinedLag > 0.0f)
            currentFreq = (float) (fs / refinedLag);
    }

    inline float autocorr (int lag) const noexcept
    {
        const int n = bufferSize;
        if (lag < 0 || lag >= n) return 0.0f;
        double sum = 0.0;
        const int count = n - lag;
        for (int i = 0; i < count; ++i)
            sum += (double) work[(size_t) i] * work[(size_t) (i + lag)];
        return (float) (sum / count);
    }

    double fs = 44100.0;
    int  bufferSize = 2646;
    int  writePos = 0;
    int  hopSize = 882, hopCounter = 0;
    std::vector<float> buffer, work;
    float currentFreq = 0.0f;
};
} // namespace apex
