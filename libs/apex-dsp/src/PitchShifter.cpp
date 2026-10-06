#include "apex/dsp/PitchShifter.h"

#include <algorithm>
#include <cmath>

namespace apex::dsp
{

void PitchShifter::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    (void) maxBlockSize;
    numPreparedChannels = std::clamp (numChannels, 1, maxChannels);

    studio.prepare (sampleRate, numPreparedChannels);
    live.prepare (sampleRate, numPreparedChannels);

    drySize = std::max (getLatencySamples (PitchMode::live), getLatencySamples (PitchMode::studio)) + 1;
    for (auto& d : dryDelay)
        d.assign ((size_t) drySize, 0.0f);

    mixStep = 1.0f / (float) std::max (1.0, 0.02 * sampleRate);   // 20 ms mix smoothing
    reset();
}

void PitchShifter::reset() noexcept
{
    studio.reset();
    live.reset();
    for (auto& d : dryDelay)
        std::fill (d.begin(), d.end(), 0.0f);
    dryWrite = 0;
    mix = mixTarget;
    updateEngines();
}

void PitchShifter::setMode (PitchMode newMode) noexcept
{
    if (newMode == mode)
        return;

    mode = newMode;
    if (mode == PitchMode::live)
        live.reset();
    else
        studio.reset();
    updateEngines();
}

void PitchShifter::setSemitones (float semitones) noexcept
{
    ratio = std::pow (2.0f, semitones / 12.0f);
    updateEngines();
}

void PitchShifter::setBody (float amount) noexcept
{
    body = std::clamp (amount, 0.0f, 1.0f);
    updateEngines();
}

void PitchShifter::updateEngines() noexcept
{
    studio.setPitchRatio (ratio);
    studio.setFormantRatio (std::pow (ratio, 1.0f - body));
    live.setPitchRatio (ratio);
    live.setFormantPreserve (ratio == 1.0f ? 0.0f : body);
}

int PitchShifter::getLatencySamples (PitchMode m) const noexcept
{
    return m == PitchMode::live ? live.getLatencySamples() : studio.getLatencySamples();
}

void PitchShifter::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, numPreparedChannels);

    // Keep a dry copy delayed by the engine latency so the blend lines up.
    const int latency = getLatencySamples();
    const bool needDry = mix < 1.0f || mixTarget < 1.0f;
    float* dryOut[maxChannels] {};
    float dryScratch[maxChannels][256];

    for (int offset = 0; offset < numSamples; offset += 256)
    {
        const int n = std::min (256, numSamples - offset);
        float* block[maxChannels] {};

        for (int ch = 0; ch < numChannels; ++ch)
        {
            block[ch] = io[ch] + offset;
            dryOut[ch] = dryScratch[ch];

            int w = dryWrite;
            auto& d = dryDelay[ch];
            for (int i = 0; i < n; ++i)
            {
                d[(size_t) w] = block[ch][i];
                int r = w - latency;
                if (r < 0)
                    r += drySize;
                dryOut[ch][i] = d[(size_t) r];
                if (++w == drySize)
                    w = 0;
            }
        }
        dryWrite = (dryWrite + n) % drySize;

        if (mode == PitchMode::live)
            live.process (block, numChannels, n);
        else
            studio.process (block, numChannels, n);

        if (needDry)
        {
            for (int i = 0; i < n; ++i)
            {
                mix += std::clamp (mixTarget - mix, -mixStep, mixStep);
                for (int ch = 0; ch < numChannels; ++ch)
                    block[ch][i] = mix * block[ch][i] + (1.0f - mix) * dryOut[ch][i];
            }
        }
    }
}

} // namespace apex::dsp
