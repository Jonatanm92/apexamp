#include "apex/dsp/StereoDelay.h"

#include <algorithm>
#include <cmath>

namespace apex::dsp
{

namespace
{
    constexpr double minDelaySamples = 4.0;

    inline float softClip (float x) noexcept
    {
        x = std::clamp (x, -3.0f, 3.0f);
        return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
    }
}

void StereoDelay::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    wowDepth = (float) (0.0003 * sampleRate);
    std::int64_t size = 1;
    while (size < (std::int64_t) (maxDelaySeconds * sampleRate) + (std::int64_t) wowDepth + 16)
        size <<= 1;
    for (auto& line : lines)
        line.assign ((size_t) size, 0.0f);
    mask = size - 1;

    send.setTime (sampleRate, 0.01);
    mixSmoother.setTime (sampleRate, 0.03);
    feedbackSmoother.setTime (sampleRate, 0.05);
    for (auto& hp : loopHighpass)
        hp.setCutoff (sampleRate, 90.0);
    duckEnvelope.setTimes (sampleRate, 0.002, 0.3);
    wow.setFrequency (sampleRate, 0.35);
    fadeLength = std::max (1, (int) std::lround (0.06 * sampleRate));

    designedTone = -1.0f;
    designTone();
    setTime (timeMs);
    reset();
}

void StereoDelay::reset() noexcept
{
    for (auto& line : lines)
        std::fill (line.begin(), line.end(), 0.0f);
    writeIndex = 0;

    headDelay[0] = headDelay[1] = targetDelay;
    active = 0;
    fading = false;
    fadePosition = 0;

    send.reset (enabled ? 1.0f : 0.0f);
    mixSmoother.reset (mix);
    feedbackSmoother.reset (feedback);
    for (auto& f : toneLowpass) f.reset();
    for (auto& f : loopHighpass) f.reset();
    duckEnvelope.reset();
    wow.setFrequency (sampleRate, 0.35);
    quietSamples = 0;
    asleep = ! enabled;
}

void StereoDelay::setTime (double milliseconds) noexcept
{
    timeMs = milliseconds;
    const double maxSamples = (double) (mask + 1) - wowDepth - 8.0;
    targetDelay = std::clamp (milliseconds * 0.001 * sampleRate, minDelaySamples, std::max (minDelaySamples, maxSamples));
}

void StereoDelay::setFeedback (float amount01) noexcept
{
    feedback = 0.95f * std::clamp (amount01, 0.0f, 1.0f);
}

void StereoDelay::setTone (float tone01) noexcept
{
    tone = std::clamp (tone01, 0.0f, 1.0f);
}

void StereoDelay::designTone() noexcept
{
    if (tone == designedTone)
        return;
    designedTone = tone;
    const double cutoff = 1200.0 * std::pow (14000.0 / 1200.0, (double) tone);
    for (auto& f : toneLowpass)
        f.setCutoff (sampleRate, std::min (cutoff, 0.45 * sampleRate));
}

float StereoDelay::readLine (const std::vector<float>& line, double delaySamples) const noexcept
{
    const double position = (double) writeIndex - delaySamples;
    const double base = std::floor (position);
    const auto i = (std::int64_t) base;
    const float t = (float) (position - base);

    const float y0 = line[(size_t) ((i - 1) & mask)];
    const float y1 = line[(size_t) (i & mask)];
    const float y2 = line[(size_t) ((i + 1) & mask)];
    const float y3 = line[(size_t) ((i + 2) & mask)];

    // 4-point Hermite
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

void StereoDelay::process (float* left, float* right, int numSamples) noexcept
{
    if (asleep)
    {
        if (! enabled)
            return;
        asleep = false;
        quietSamples = 0;
    }

    designTone();
    float blockPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = left[i];
        const float inR = right != nullptr ? right[i] : inL;
        const float s = send.process (enabled ? 1.0f : 0.0f);

        if (! fading && std::abs (targetDelay - headDelay[active]) > 0.5)
        {
            headDelay[1 - active] = targetDelay;
            fading = true;
            fadePosition = 0;
        }

        const double modulation = wowDepth * (0.5 + 0.5 * wow.next());
        float wetL = readLine (lines[0], headDelay[active] + modulation);
        float wetR = readLine (lines[1], headDelay[active] + modulation);

        if (fading)
        {
            const float t = (float) fadePosition / (float) fadeLength;
            const float w = 0.5f - 0.5f * std::cos ((float) kPi * t);
            const int other = 1 - active;
            wetL += w * (readLine (lines[0], headDelay[other] + modulation) - wetL);
            wetR += w * (readLine (lines[1], headDelay[other] + modulation) - wetR);
            if (++fadePosition >= fadeLength)
            {
                active = other;
                fading = false;
            }
        }

        // Each pass loses top end and sub-lows, with tape-like soft saturation.
        float loop[2] { wetL, wetR };
        for (int ch = 0; ch < 2; ++ch)
        {
            const float dark = toneLowpass[ch].process (loop[ch]);
            loop[ch] = softClip (dark - loopHighpass[ch].process (dark));
        }

        const float fb = feedbackSmoother.process (feedback);
        float writeL, writeR;
        if (pingPong)
        {
            writeL = s * 0.5f * (inL + inR) + fb * loop[1];
            writeR = fb * loop[0];
        }
        else
        {
            writeL = s * inL + fb * loop[0];
            writeR = s * inR + fb * loop[1];
        }
        lines[0][(size_t) (writeIndex & mask)] = writeL;
        lines[1][(size_t) (writeIndex & mask)] = writeR;
        ++writeIndex;

        const float env = duckEnvelope.process (std::max (std::abs (inL), std::abs (inR)));
        const float g = mixSmoother.process (mix) / (1.0f + duck * 12.0f * env);

        if (right != nullptr)
        {
            left[i]  = inL + g * wetL;
            right[i] = inR + g * wetR;
        }
        else
        {
            left[i] = inL + g * (wetL + wetR);
        }
        blockPeak = std::max (blockPeak, std::max (std::abs (wetL), std::abs (wetR)));
    }

    // Once off and silent for longer than the longest repeat, stop processing.
    if (! enabled && send.z < 1.0e-6f && blockPeak < 1.0e-6f)
    {
        quietSamples += numSamples;
        if (quietSamples > (int) (maxDelaySeconds * sampleRate) + fadeLength)
        {
            for (auto& line : lines)
                std::fill (line.begin(), line.end(), 0.0f);
            asleep = true;
        }
    }
    else
    {
        quietSamples = 0;
    }
}

} // namespace apex::dsp
