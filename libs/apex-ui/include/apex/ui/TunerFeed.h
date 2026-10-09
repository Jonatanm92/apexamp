#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <vector>

namespace apex::ui
{

/**
 * TunerFeed
 * ---------
 * Lock-free bridge from the audio thread to the tuner display. The processor
 * pushes its input every block; nothing is copied unless the tuner is open.
 * Audio is averaged to mono and decimated to ~12 kHz, which is plenty for
 * guitar fundamentals down to a 9-string low end.
 */
class TunerFeed
{
public:
    void prepare (double sampleRate)
    {
        decimation = juce::jmax (1, juce::roundToInt (sampleRate / 12000.0));
        rate = sampleRate / (double) decimation;
        accumulator = 0.0f;
        phase = 0;
        fifo.reset();
    }

    void push (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        if (! active.load (std::memory_order_relaxed) || numChannels <= 0)
            return;

        const float channelScale = 1.0f / (float) numChannels;
        for (int i = 0; i < numSamples; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                s += channels[ch][i];
            accumulator += s * channelScale;

            if (++phase >= decimation)
            {
                const float value = accumulator / (float) decimation;
                accumulator = 0.0f;
                phase = 0;
                const auto scope = fifo.write (1);
                if (scope.blockSize1 > 0)      buffer[(size_t) scope.startIndex1] = value;
                else if (scope.blockSize2 > 0) buffer[(size_t) scope.startIndex2] = value;
            }
        }
    }

    /** Reader side (message thread). Returns the number of samples copied. */
    int read (float* dest, int maxSamples) noexcept
    {
        const auto scope = fifo.read (juce::jmin (maxSamples, fifo.getNumReady()));
        for (int i = 0; i < scope.blockSize1; ++i) dest[i] = buffer[(size_t) (scope.startIndex1 + i)];
        for (int i = 0; i < scope.blockSize2; ++i) dest[scope.blockSize1 + i] = buffer[(size_t) (scope.startIndex2 + i)];
        return scope.blockSize1 + scope.blockSize2;
    }

    double getRate() const noexcept { return rate; }

    /** Set by the tuner UI: collect samples / mute the plugin output. */
    std::atomic<bool> active { false };
    std::atomic<bool> mute   { false };

private:
    juce::AbstractFifo fifo { 16384 };
    std::vector<float> buffer = std::vector<float> (16384, 0.0f);
    double rate = 12000.0;
    int decimation = 4, phase = 0;
    float accumulator = 0.0f;
};

} // namespace apex::ui
