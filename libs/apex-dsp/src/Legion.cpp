#include "apex/dsp/Legion.h"

#include <algorithm>
#include <cmath>

namespace apex::dsp
{

//==============================================================================
void RiffFollower::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    highpass = Biquad::highpass (sampleRate, 40.0);
    low1 = Biquad::lowpass (sampleRate, 120.0);
    low2 = Biquad::lowpass (sampleRate, 120.0);
    full.fast.setTimes (sampleRate, 0.0003, 0.008);
    full.slow.setTimes (sampleRate, 0.025, 0.15);
    low.fast.setTimes (sampleRate, 0.001, 0.012);
    low.slow.setTimes (sampleRate, 0.025, 0.15);
    loudestDecay = (float) std::exp (-1.0 / (2.0 * sampleRate));
    minInterval = (int) std::lround (0.045 * sampleRate);
    confirmWindow = (int) std::lround (0.014 * sampleRate);
    maxBackdate = (int) std::lround (0.015 * sampleRate);
    reset();
}

void RiffFollower::reset() noexcept
{
    highpass.reset();
    low1.reset();
    low2.reset();
    full.reset();
    low.reset();
    loudest = 1.0e-3f;
    lowLoudest = 1.0e-4f;
    sinceHit = 1 << 30;
    pending = false;
    rising = 0;
    previousFast = 0.0f;
}

int RiffFollower::process (const float* di, int numSamples) noexcept
{
    int count = 0;
    const bool chugs = mode == Mode::chugs;
    // attack ratio (short-term over long-term envelope) needed for an attack
    const float threshold = 1.4f + 1.6f * (1.0f - sensitivity);
    const float rearm = std::max (1.15f, 0.75f * threshold);

    for (int i = 0; i < numSamples; ++i)
    {
        const float x = highpass.process (di[i]);
        const float l = low2.process (low1.process (x));
        const float ff = full.fast.process (x), fsl = full.slow.process (x);
        const float lf = low.fast.process (l);
        loudest = std::max (ff, loudest * loudestDecay);
        lowLoudest = std::max (lf, lowLoudest * loudestDecay);
        if (sinceHit < (1 << 30))
            ++sinceHit;
        rising = ff > previousFast * 1.0005f ? std::min (rising + 1, maxBackdate) : 0;
        previousFast = ff;

        const float ratio = ff / (fsl + 1.0e-9f);
        if (! full.armed && ratio < rearm)
            full.armed = true;

        const bool attack = full.armed && ratio > threshold && ff > 0.05f * loudest && ff > 1.0e-4f && sinceHit >= minInterval;
        if (attack)
        {
            full.armed = false;
            sinceHit = 0;
            const float v = std::sqrt (std::clamp (ff / (loudest + 1.0e-9f), 0.1f, 1.0f));
            if (! chugs)
            {
                if (count < maxHitsPerBlock)
                    hits[(size_t) count++] = { i, v, rising };
            }
            else
            {
                pending = true;
                pendingAge = 0;
                pendingRise = rising;
                pendingLowStart = lf;
                pendingVelocity = v;
            }
        }

        if (pending)
        {
            // a low note: the low band rises clearly and carries a good share of
            // what the riff's low notes usually have; a lead note does neither
            if (lf > 1.35f * pendingLowStart + 1.0e-6f && lf > 0.25f * lowLoudest && lf > 1.0e-5f)
            {
                if (count < maxHitsPerBlock)
                    hits[(size_t) count++] = { i, pendingVelocity, pendingAge + pendingRise };
                pending = false;
            }
            else if (++pendingAge > confirmWindow)
            {
                pending = false;
            }
        }
    }
    return count;
}

//==============================================================================
void KickSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (auto& v : voices)
        v.clickFilter = Biquad::bandpass (sampleRate, std::min (3800.0, 0.4 * sampleRate), 1.2);
    reset();
}

void KickSynth::reset() noexcept
{
    for (auto& v : voices)
    {
        v.phase = 0.0;
        v.t = 0.0f;
        v.amp = 0.0f;
        v.release = 1.0f;
        v.releasing = false;
        v.clickFilter.reset();
    }
    nextVoice = 0;
}

void KickSynth::trigger (float velocity) noexcept
{
    auto& other = voices[(size_t) (1 - nextVoice)];
    if (other.amp > 1.0e-5f)
        other.releasing = true;   // fade the previous hit out under the new one

    auto& v = voices[(size_t) nextVoice];
    v.phase = 0.0;
    v.t = 0.0f;
    v.amp = 1.0f;
    v.release = 1.0f;
    v.releasing = false;
    v.velocity = 0.35f + 0.65f * std::clamp (velocity, 0.0f, 1.0f);
    v.noise = v.noise * 747796405u + 2891336453u;
    nextVoice = 1 - nextVoice;
}

float KickSynth::next() noexcept
{
    const float dt = (float) (1.0 / sampleRate);
    const float releaseStep = (float) std::exp (-1.0 / (0.004 * sampleRate));
    const float f0 = 46.0f + 16.0f * tone, f1 = 190.0f + 130.0f * tone;
    const float bodyTau = 0.075f - 0.03f * tone, clickGain = 0.45f + 0.8f * tone;

    float out = 0.0f;
    for (auto& v : voices)
    {
        if (v.amp <= 1.0e-5f)
            continue;
        const float t = v.t;
        const float f = f0 + (f1 - f0) * std::exp (-t / 0.011f);
        v.phase += 2.0 * kPi * (double) f / sampleRate;
        if (v.phase > 2.0 * kPi)
            v.phase -= 2.0 * kPi;
        const float bodyEnv = std::exp (-t / bodyTau) * std::min (1.0f, t / 0.0004f + 0.05f);
        const float body = (float) std::sin (v.phase) * bodyEnv;

        v.noise = v.noise * 1664525u + 1013904223u;
        const float white = (float) (v.noise >> 8) * (2.0f / 16777216.0f) - 1.0f;
        const float click = v.clickFilter.process (white) * std::exp (-t / 0.005f) * clickGain;
        const float beater = white * std::exp (-t / 0.0008f) * 0.35f * clickGain;

        float x = (body + click + beater) * v.velocity;
        if (v.releasing)
        {
            v.release *= releaseStep;
            x *= v.release;
        }
        v.amp = bodyEnv * v.release + (t < 0.01f ? 1.0f : 0.0f);
        v.t = t + dt;
        out += x;
    }
    return 0.85f * std::tanh (1.5f * out) / std::tanh (1.5f);
}

//==============================================================================
void BassFollower::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    shifter.prepare (sampleRate, maxBlockSize, 1);
    shifter.setMode (PitchMode::live);
    shifter.setBody (0.0f);   // a bass needs no guitar formants (and this saves the corrector)
    shifter.setMix (1.0f);
    inputLow = Biquad::lowpass (sampleRate, 1500.0);
    subLow1 = Biquad::lowpass (sampleRate, 180.0);
    subLow2 = Biquad::lowpass (sampleRate, 180.0);
    midHigh1 = Biquad::highpass (sampleRate, 180.0);
    midHigh2 = Biquad::highpass (sampleRate, 180.0);
    midLow = Biquad::lowpass (sampleRate, 2600.0);
    gritSmoother.setTime (sampleRate, 0.03);
    reset();
}

void BassFollower::reset() noexcept
{
    shifter.reset();
    for (auto* f : { &inputLow, &subLow1, &subLow2, &midHigh1, &midHigh2, &midLow })
        f->reset();
    gritSmoother.reset (grit);
}

void BassFollower::process (const float* di, float* out, int numSamples) noexcept
{
    // pick fizz and string noise off before the shift: a bass has none of it
    for (int i = 0; i < numSamples; ++i)
        out[i] = inputLow.process (di[i]);

    shifter.setSemitones (semitones);
    float* channels[1] { out };
    shifter.process (channels, 1, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = gritSmoother.process (grit);
        const float x = out[i];
        const float sub = subLow2.process (subLow1.process (x));
        const float mid = midHigh2.process (midHigh1.process (x));
        const float drive = 1.0f + 40.0f * g;
        const float driven = midLow.process (std::tanh (mid * drive) / std::sqrt (drive));
        out[i] = 1.5f * sub + (1.0f - g) * 0.8f * mid + g * 1.1f * driven;
    }
}

} // namespace apex::dsp
