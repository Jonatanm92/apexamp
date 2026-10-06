#include "apex/dsp/SplicingShifter.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace apex::dsp
{

namespace
{
    // Accept the shortest jump whose correlation is within 10 % of the best one.
    constexpr float goodMatch = 0.9f;
}

void SplicingShifter::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    numPreparedChannels = std::clamp (numChannels, 1, maxChannels);

    crossfadeLength = std::max (16, (int) std::lround (0.006 * sampleRate));
    minJump  = std::max (8, (int) std::lround (0.0025 * sampleRate));     // 400 Hz period
    maxJump  = (int) std::lround (0.028 * sampleRate);                    // ~36 Hz period (low B / F# / E)
    decimation = std::max (1, (int) std::lround (sampleRate / 12000.0));
    attackCushion = (int) std::lround (0.002 * sampleRate);
    nominalDelay = crossfadeLength + 4 + attackCushion;
    refractory = (int) std::lround (0.05 * sampleRate);
    fastCoeff = (float) (1.0 - std::exp (-1.0 / (0.001 * sampleRate)));
    slowCoeff = (float) (1.0 - std::exp (-1.0 / (0.03 * sampleRate)));

    // Room for the deepest read head (fast upshift right after a long jump),
    // plus the correlation windows around it.
    const int needed = 4 * (3 * maxJump + 4 * crossfadeLength + nominalDelay);
    int size = 1;
    while (size < needed)
        size <<= 1;
    mask = size - 1;

    for (auto& b : buffers)
        b.assign ((size_t) size, 0.0f);
    mono.assign ((size_t) size, 0.0f);

    fade.resize ((size_t) crossfadeLength);
    constexpr double pi = 3.14159265358979323846;
    for (int i = 0; i < crossfadeLength; ++i)
        fade[(size_t) i] = (float) (0.5 - 0.5 * std::cos (pi * (i + 0.5) / crossfadeLength));

    coarse.assign ((size_t) (maxJump + 2), 0.0f);
    retryInterval = std::max (16, (int) std::lround (0.0027 * sampleRate));

    formant.prepare (sampleRate, numPreparedChannels);
    sourceWindow.assign ((size_t) formant.getAnalysisLength(), 0.0f);
    reset();
}

void SplicingShifter::reset() noexcept
{
    for (auto& b : buffers)
        std::fill (b.begin(), b.end(), 0.0f);
    std::fill (mono.begin(), mono.end(), 0.0f);

    writeIndex  = 0;
    delay       = (double) nominalDelay;
    spliceDelay = delay;
    splicing    = false;
    spliceCounter = 0;
    spliceCount = 0;
    lastJump    = (int) std::lround (0.01 * sampleRate);
    retryCountdown = 0;
    fastEnergy = slowEnergy = previousMono = 0.0f;
    sinceAttack = 0;
    pendingCatchUp = false;
    formant.reset();
}

void SplicingShifter::setPitchRatio (float newRatio) noexcept
{
    ratio = std::clamp (newRatio, 0.25f, 4.0f);
}

int SplicingShifter::minimumDelay() const noexcept
{
    // The old head must be able to read a full crossfade ahead without passing
    // the write head; 4 extra samples cover the cubic interpolator.
    return (int) std::ceil ((double) crossfadeLength * std::max (1.0f, ratio)) + 4;
}

float SplicingShifter::readBuffer (const std::vector<float>& b, double position) const noexcept
{
    const double base = std::floor (position);
    const auto i = (std::int64_t) base;
    const float t = (float) (position - base);

    const float xm1 = b[(size_t) ((i - 1) & mask)];
    const float x0  = b[(size_t) (i & mask)];
    const float x1  = b[(size_t) ((i + 1) & mask)];
    const float x2  = b[(size_t) ((i + 2) & mask)];

    // 4-point, 3rd-order Hermite (Catmull-Rom).
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

float SplicingShifter::correlation (std::int64_t a, std::int64_t b, int length, int stride) const noexcept
{
    double dot = 0.0, ea = 0.0, eb = 0.0;
    for (int n = 0; n < length; n += stride)
    {
        const double x = mono[(size_t) ((a + n) & mask)];
        const double y = mono[(size_t) ((b + n) & mask)];
        dot += x * y;
        ea  += x * x;
        eb  += y * y;
    }
    return (float) (dot / std::sqrt (ea * eb + 1.0e-12));
}

void SplicingShifter::maybeStartSplice() noexcept
{
    const int minDelay = minimumDelay();
    int direction = 0, maxL = 0;
    bool mustSplice = true;

    if (ratio < 1.0f)
    {
        // Downshift: the read head falls behind. Wait until roughly one more
        // period than last time is available, then jump forwards.
        const int trigger = minDelay + std::clamp ((int) (1.3f * (float) lastJump), 2 * minJump, maxJump);
        if (delay < (double) trigger)
            return;
        if (retryCountdown > 0)
        {
            --retryCountdown;
            return;
        }
        direction = 1;
        maxL = std::min (maxJump, (int) std::floor (delay) - minDelay);
        mustSplice = maxL >= maxJump;
    }
    else if (ratio > 1.0f)
    {
        // Upshift: the read head catches up. Jump backwards before it does.
        if (delay > (double) minDelay)
            return;
        direction = -1;
        maxL = std::min (maxJump, (int) (mask + 1) - (int) std::ceil (delay) - 2 * maxJump - 4 * crossfadeLength - 8);
    }
    else
    {
        // Back at unity: return to the reported latency once.
        if (std::abs (delay - (double) nominalDelay) < 0.5)
            return;
        spliceDelay = (double) nominalDelay;
        splicing = true;
        spliceCounter = 0;
        ++spliceCount;
        return;
    }

    if (maxL < minJump)
        return;

    // Compare what the two heads have just played (one longest period of
    // history) plus what they are about to play during the crossfade. A window
    // shorter than a period matches at fractions of it and the splice lands
    // out of phase on low notes.
    const int ahead  = (int) std::ceil ((double) crossfadeLength * ratio);
    const int length = maxJump + ahead;
    const auto start = (std::int64_t) std::floor ((double) writeIndex - delay) - maxJump;
    const int lagStep = std::max (1, decimation / 2);

    // Silence: nothing to hear, so take the jump that best restores latency.
    double energy = 0.0;
    for (int n = 0; n < length; n += decimation)
    {
        const double x = mono[(size_t) ((start + n) & mask)];
        energy += x * x;
    }
    if (energy * decimation / length < 1.0e-8)    // below ~ -80 dBFS RMS
    {
        const int L = direction > 0 ? maxL : minJump;
        spliceDelay = delay - (double) (direction * L);
        splicing = true;
        spliceCounter = 0;
        ++spliceCount;
        return;
    }

    // Coarse pass on a decimated grid.
    int count = 0;
    float best = -std::numeric_limits<float>::infinity();
    int bestIndex = 0;
    for (int L = minJump; L <= maxL; L += lagStep)
    {
        const float c = correlation (start, start + direction * L, length, decimation);
        if (c > best) { best = c; bestIndex = count; }
        coarse[(size_t) count++] = c;
    }

    // Shortest jump that is nearly as good as the best one (~one period).
    int chosen = bestIndex;
    for (int j = 0; j < count; ++j)
    {
        const float c = coarse[(size_t) j];
        const bool localMax = (j == 0 || c >= coarse[(size_t) j - 1])
                           && (j + 1 == count || c >= coarse[(size_t) j + 1]);
        if (localMax && c >= goodMatch * best)
        {
            chosen = j;
            break;
        }
    }

    // Nothing period-like in reach yet (a lower note than last time): let the
    // delay grow a little and look again, up to the longest period.
    if (! mustSplice && best < 0.8f)
    {
        retryCountdown = retryInterval;
        return;
    }

    // Fine pass at full resolution around the chosen lag.
    const int centre = minJump + chosen * lagStep;
    int bestL = centre;
    float bestFine = -std::numeric_limits<float>::infinity();
    for (int L = std::max (minJump, centre - lagStep); L <= std::min (maxL, centre + lagStep); ++L)
    {
        const float c = correlation (start, start + direction * L, length, 1);
        if (c > bestFine) { bestFine = c; bestL = L; }
    }

    spliceDelay = delay - (double) (direction * bestL);
    splicing = true;
    spliceCounter = 0;
    ++spliceCount;
    lastJump = bestL;
}

void SplicingShifter::catchUpToAttack() noexcept
{
    pendingCatchUp = false;
    const double target = (double) (minimumDelay() + attackCushion);
    if (delay <= target + (double) attackCushion)
        return;

    // The old head is still in the previous note's tail; the new one starts
    // just before the attack, which then plays at full level after the fade.
    spliceDelay = target;
    splicing = true;
    spliceCounter = 0;
    ++spliceCount;
    retryCountdown = 0;
}

void SplicingShifter::process (float* const* io, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, numPreparedChannels);
    float frame[maxChannels] {};
    const double drift = 1.0 - (double) ratio;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto idx = (size_t) (writeIndex & mask);
        float sum = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffers[ch][idx] = io[ch][i];
            sum += io[ch][i];
        }
        const float monoSample = sum / (float) numChannels;
        mono[idx] = monoSample;

        // Pick-attack detector on the input (differentiated, fast vs slow energy).
        const float slope = monoSample - previousMono;
        previousMono = monoSample;
        fastEnergy += fastCoeff * (slope * slope - fastEnergy);
        slowEnergy += slowCoeff * (slope * slope - slowEnergy);
        if (++sinceAttack > refractory && fastEnergy > 6.0f * slowEnergy && fastEnergy > 1.0e-7f)
        {
            sinceAttack = 0;
            pendingCatchUp = true;
        }

        const double position = (double) writeIndex - delay;
        if (splicing)
        {
            const float g = fade[(size_t) spliceCounter];
            const double incoming = (double) writeIndex - spliceDelay;
            for (int ch = 0; ch < numChannels; ++ch)
                frame[ch] = (1.0f - g) * readBuffer (buffers[ch], position)
                          + g * readBuffer (buffers[ch], incoming);

            if (++spliceCounter >= crossfadeLength)
            {
                splicing = false;
                delay = spliceDelay;
            }
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
                frame[ch] = readBuffer (buffers[ch], position);
        }

        // The formant target is the unshifted content (read at the original
        // rate) ending under the read head, so both envelope fits see the
        // same notes.
        if (formant.wantsSource())
        {
            const auto end = (std::int64_t) std::floor (position);
            const int length = (int) sourceWindow.size();
            for (int n = 0; n < length; ++n)
                sourceWindow[(size_t) n] = mono[(size_t) ((end - length + 1 + n) & mask)];
            formant.setSource (sourceWindow.data());
        }
        formant.process (frame, numChannels);

        for (int ch = 0; ch < numChannels; ++ch)
            io[ch][i] = frame[ch];

        ++writeIndex;
        delay += drift;
        if (splicing)
            spliceDelay += drift;
        else if (pendingCatchUp)
            catchUpToAttack();
        else
            maybeStartSplice();
    }
}

} // namespace apex::dsp
