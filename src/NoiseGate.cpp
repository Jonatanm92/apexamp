#include "NoiseGate.h"
#include <algorithm>

namespace namapp {
namespace dsp {

NoiseGate::NoiseGate() {
    recalculateCoefficients();
}

void NoiseGate::prepare(double sampleRate, int samplesPerBlock) {
    mSampleRate = sampleRate;
    mLookaheadBuffer.resize(samplesPerBlock, 0.0f);
    recalculateCoefficients();
}

void NoiseGate::setThreshold(float thresholdDB) {
    mThresholdDB = thresholdDB;
    mThresholdLinear = std::pow(10.0f, thresholdDB / 20.0f);
}

void NoiseGate::setAttack(float attackMs) {
    mAttackMs = attackMs;
    recalculateCoefficients();
}

void NoiseGate::setRelease(float releaseMs) {
    mReleaseMs = releaseMs;
    recalculateCoefficients();
}

void NoiseGate::recalculateCoefficients() {
    mAttackCoeff = std::exp(-1.0f / (mAttackMs * 0.001f * static_cast<float>(mSampleRate)));
    mReleaseCoeff = std::exp(-1.0f / (mReleaseMs * 0.001f * static_cast<float>(mSampleRate)));
}

void NoiseGate::process(std::vector<float>& buffer) {
    // Basic lookahead gate algorithm
    for (size_t i = 0; i < buffer.size(); ++i) {
        float input = buffer[i];
        float absInput = std::abs(input);
        
        // Simple envelope follower
        if (absInput > mEnvelope) {
            mEnvelope = mAttackCoeff * mEnvelope + (1.0f - mAttackCoeff) * absInput;
        } else {
            mEnvelope = mReleaseCoeff * mEnvelope + (1.0f - mReleaseCoeff) * absInput;
        }
        
        // Gate logic
        float gain = (mEnvelope > mThresholdLinear) ? 1.0f : 0.0f;
        buffer[i] *= gain;
    }
}

} // namespace dsp
} // namespace namapp
