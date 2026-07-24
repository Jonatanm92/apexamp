#pragma once

#include <vector>
#include <cmath>

namespace namapp {
namespace dsp {

class NoiseGate {
public:
    NoiseGate();
    
    void prepare(double sampleRate, int samplesPerBlock);
    void process(std::vector<float>& buffer);

    void setThreshold(float thresholdDB);
    void setAttack(float attackMs);
    void setRelease(float releaseMs);
    void setLookahead(int lookaheadSamples);

private:
    double mSampleRate{44100.0};
    float mThresholdDB{-60.0f};
    float mThresholdLinear{0.001f};
    
    float mAttackMs{1.0f};
    float mReleaseMs{50.0f};
    
    float mAttackCoeff{0.0f};
    float mReleaseCoeff{0.0f};
    
    float mEnvelope{0.0f};
    
    int mLookaheadSamples{0};
    std::vector<float> mLookaheadBuffer;
    int mLookaheadIndex{0};

    void recalculateCoefficients();
};

} // namespace dsp
} // namespace namapp
