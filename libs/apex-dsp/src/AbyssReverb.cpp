#include "apex/dsp/AbyssReverb.h"

#include <cmath>

namespace apex::dsp
{

namespace
{
    // Tank delays (ms): spread over ~1.5 octaves, no common factors at 48 kHz.
    constexpr double lineMs[AbyssReverb::numLines] = { 29.3, 35.9, 43.1, 50.6, 58.7, 67.3, 76.1, 85.9 };
    constexpr double diffuserMs[2][4] = { { 4.77, 3.59, 12.73, 9.31 }, { 4.91, 3.37, 12.11, 9.83 } };

    // Injection signs per line (L on even lines, R on odd) and two output taps
    // orthogonal to each other and to the all-ones vector the Householder
    // matrix reflects.
    constexpr float injectSign[AbyssReverb::numLines] = { 1, 1, -1, -1, 1, -1, -1, 1 };
    constexpr float tapLeft[AbyssReverb::numLines]    = { 1, -1, 1, -1, 1, -1, 1, -1 };
    constexpr float tapRight[AbyssReverb::numLines]   = { 1, 1, -1, -1, 1, 1, -1, -1 };

    constexpr double preDelaySeconds = 0.018;
    constexpr float inputScale = 0.5f;

    inline float softClip (float x) noexcept
    {
        x = std::clamp (x, -3.0f, 3.0f);
        return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
    }
}

void AbyssReverb::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    modulationDepth = (float) (0.0004 * sampleRate);
    double longest = 0.0;
    for (int j = 0; j < numLines; ++j)
    {
        lineLength[(size_t) j] = std::round (lineMs[j] * 0.001 * sampleRate);
        longest = std::max (longest, lineLength[(size_t) j]);
    }
    std::int64_t size = 1;
    while (size < (std::int64_t) (longest + modulationDepth) + 8)
        size <<= 1;
    lineMask = size - 1;
    for (auto& line : lines)
        line.assign ((size_t) size, 0.0f);

    for (int j = 0; j < numLines; ++j)
        modulators[(size_t) j].setFrequency (sampleRate, 0.11 + 0.057 * j, 0.7 * j);

    for (auto& p : preDelay)
        p.assign ((size_t) std::max (1, (int) std::lround (preDelaySeconds * sampleRate)), 0.0f);
    for (int ch = 0; ch < 2; ++ch)
    {
        inputHighpass[ch]  = Biquad::highpass (sampleRate, 150.0);
        outputHighpass[ch] = Biquad::highpass (sampleRate, 70.0);
        for (int k = 0; k < 4; ++k)
            diffusers[ch][k].prepare ((int) std::lround (diffuserMs[ch][k] * 0.001 * sampleRate));
    }

    send.setTime (sampleRate, 0.01);
    mixSmoother.setTime (sampleRate, 0.03);
    abyssSmoother.setTime (sampleRate, 0.05);

    octaveDown.prepare (sampleRate);
    shimmerHighpass = Biquad::highpass (sampleRate, 160.0);
    shimmerLowpass  = Biquad::lowpass (sampleRate, 3500.0);

    sleepAfter = (int) std::lround (1.0 * sampleRate);
    designedTone = -1.0f;
    updateTone();
    reset();
}

void AbyssReverb::reset() noexcept
{
    for (auto& line : lines)
        std::fill (line.begin(), line.end(), 0.0f);
    for (auto& a : absorb) a.reset();
    for (int j = 0; j < numLines; ++j)
        modulators[(size_t) j].setFrequency (sampleRate, 0.11 + 0.057 * j, 0.7 * j);
    writeIndex = 0;

    for (auto& p : preDelay)
        std::fill (p.begin(), p.end(), 0.0f);
    preDelayIndex = 0;
    for (int ch = 0; ch < 2; ++ch)
    {
        inputHighpass[ch].reset();
        inputLowpass[ch].reset();
        outputHighpass[ch].reset();
        for (auto& d : diffusers[ch]) d.reset();
    }

    send.reset (enabled ? 1.0f : 0.0f);
    mixSmoother.reset (mix);
    abyssSmoother.reset (abyss);

    octaveDown.reset();
    shimmerHighpass.reset();
    shimmerLowpass.reset();
    shimmer = 0.0f;

    quietSamples = 0;
    asleep = ! enabled;
}

void AbyssReverb::setDecay (float seconds) noexcept
{
    decay = std::clamp (seconds, 0.3f, 30.0f);
}

void AbyssReverb::setAbyss (float amount01) noexcept
{
    abyss = std::clamp (amount01, 0.0f, 1.0f);
}

void AbyssReverb::setTone (float tone01) noexcept
{
    tone = std::clamp (tone01, 0.0f, 1.0f);
}

void AbyssReverb::updateTone() noexcept
{
    if (tone == designedTone)
        return;
    designedTone = tone;

    // Absorption in the tank 2 .. 12 kHz, input band limit 4 .. 12 kHz.
    const double absorbHz = std::min (2000.0 * std::pow (6.0, (double) tone), 0.45 * sampleRate);
    const double inputHz  = std::min (4000.0 * std::pow (3.0, (double) tone), 0.45 * sampleRate);
    for (auto& a : absorb)
        a.setCutoff (sampleRate, absorbHz);
    for (auto& f : inputLowpass)
        f.copyCoefficients (Biquad::lowpass (sampleRate, inputHz));

    designedDecay = -1.0f;   // the decay compensation depends on the absorption
    updateDecay();
}

void AbyssReverb::updateDecay() noexcept
{
    if (decay == designedDecay)
        return;
    designedDecay = decay;

    // The absorption low-pass also takes a little off the mids on every pass;
    // compensate so the decay time is exact in the mids (highs still die faster).
    const double a = absorb[0].a, w = 2.0 * kPi * 600.0 / sampleRate;
    const double midLoss = a / std::sqrt (1.0 - 2.0 * (1.0 - a) * std::cos (w) + (1.0 - a) * (1.0 - a));

    double meanGainSquared = 0.0;
    for (int j = 0; j < numLines; ++j)
    {
        const double g = std::min (0.9995, std::pow (10.0, -3.0 * lineLength[(size_t) j] / (decay * sampleRate)) / midLoss);
        lineGain[(size_t) j] = (float) g;
        meanGainSquared += g * g / numLines;
    }

    // Energy gain of the tank. Longer decays still sound bigger, but the level
    // only grows gently with the setting; the abyss feedback is normalised by
    // the full gain so its loop stays below unity.
    const double energy = 1.0 / std::max (1.0e-6, 1.0 - meanGainSquared);
    wetScale = (float) (std::pow (energy, -0.3) / std::sqrt ((double) numLines));
    shimmerScale = (float) (0.7 / std::sqrt (energy));
}

float AbyssReverb::readLine (int line, double delaySamples) const noexcept
{
    const auto& buffer = lines[(size_t) line];
    const double position = (double) writeIndex - delaySamples;
    const double base = std::floor (position);
    const auto i = (std::int64_t) base;
    const float t = (float) (position - base);
    const float a = buffer[(size_t) (i & lineMask)];
    const float b = buffer[(size_t) ((i + 1) & lineMask)];
    return a + t * (b - a);
}

//==============================================================================
void AbyssReverb::OctaveDown::prepare (double sampleRate)
{
    // Heads drift from `minimum` to `minimum + span` samples behind the input
    // at half speed, so one grain lasts 2 * span samples.
    minimum = 4.0;
    span = std::round (0.045 * sampleRate);
    phaseStep = 1.0 / (2.0 * span);
    std::int64_t size = 1;
    while (size < (std::int64_t) (minimum + span) + 8)
        size <<= 1;
    buffer.assign ((size_t) size, 0.0f);
    mask = size - 1;
    reset();
}

void AbyssReverb::OctaveDown::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
    phase = 0.0;
}

float AbyssReverb::OctaveDown::read (double delay) const noexcept
{
    const double position = (double) writeIndex - delay;
    const double base = std::floor (position);
    const auto i = (std::int64_t) base;
    const float t = (float) (position - base);
    const float a = buffer[(size_t) (i & mask)];
    const float b = buffer[(size_t) ((i + 1) & mask)];
    return a + t * (b - a);
}

float AbyssReverb::OctaveDown::process (float x) noexcept
{
    buffer[(size_t) (writeIndex & mask)] = x;
    ++writeIndex;

    const double second = phase < 0.5 ? phase + 0.5 : phase - 0.5;
    const float y = (float) std::sin (kPi * phase) * read (minimum + span * phase)
                  + (float) std::sin (kPi * second) * read (minimum + span * second);

    phase += phaseStep;
    if (phase >= 1.0)
        phase -= 1.0;
    return y;
}

void AbyssReverb::process (float* left, float* right, int numSamples) noexcept
{
    if (asleep)
    {
        if (! enabled)
            return;
        asleep = false;
        quietSamples = 0;
    }

    updateTone();
    updateDecay();
    float blockPeak = 0.0f;
    float out[numLines];
    const float mixing = 2.0f / (float) numLines;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = left[i];
        const float inR = right != nullptr ? right[i] : inL;
        const float s = send.process (enabled ? 1.0f : 0.0f);

        // pre-delay and band-limit the input
        float xL = preDelay[0][(size_t) preDelayIndex];
        float xR = preDelay[1][(size_t) preDelayIndex];
        preDelay[0][(size_t) preDelayIndex] = s * inL;
        preDelay[1][(size_t) preDelayIndex] = s * inR;
        if (++preDelayIndex >= (int) preDelay[0].size())
            preDelayIndex = 0;
        xL = inputLowpass[0].process (inputHighpass[0].process (xL));
        xR = inputLowpass[1].process (inputHighpass[1].process (xR));

        // the abyss: the tail an octave down, back into the tank
        const float depth = abyssSmoother.process (abyss);
        if (depth > 1.0e-5f)
        {
            const float down = softClip (octaveDown.process (shimmer) * depth * shimmerScale);
            xL += down;
            xR += down;
        }

        for (auto& d : diffusers[0]) xL = d.process (xL);
        for (auto& d : diffusers[1]) xR = d.process (xR);

        // tank
        float sum = 0.0f;
        for (int j = 0; j < numLines; ++j)
        {
            const double wobble = modulationDepth * (0.5 + 0.5 * modulators[(size_t) j].next());
            out[j] = readLine (j, lineLength[(size_t) j] + wobble);
            sum += out[j];
        }
        const float reflect = sum * mixing;
        float wetL = 0.0f, wetR = 0.0f;
        for (int j = 0; j < numLines; ++j)
        {
            const float mixed = absorb[(size_t) j].process (out[j] - reflect) * lineGain[(size_t) j];
            const float injected = injectSign[j] * inputScale * ((j & 1) == 0 ? xL : xR);
            lines[(size_t) j][(size_t) (writeIndex & lineMask)] = mixed + injected;
            wetL += tapLeft[j] * out[j];
            wetR += tapRight[j] * out[j];
        }
        ++writeIndex;

        shimmer = shimmerLowpass.process (shimmerHighpass.process (sum * 0.35f));

        wetL = outputHighpass[0].process (wetL * wetScale);
        wetR = outputHighpass[1].process (wetR * wetScale);
        const float g = mixSmoother.process (mix);

        if (right != nullptr)
        {
            left[i]  = inL + g * wetL;
            right[i] = inR + g * wetR;
        }
        else
        {
            left[i] = inL + g * 0.7071f * (wetL + wetR);
        }
        blockPeak = std::max (blockPeak, std::max (std::abs (wetL), std::abs (wetR)));
    }

    if (! enabled && send.z < 1.0e-6f && blockPeak < 1.0e-6f)
    {
        quietSamples += numSamples;
        if (quietSamples > sleepAfter)
            reset();
    }
    else
    {
        quietSamples = 0;
    }
}

} // namespace apex::dsp
