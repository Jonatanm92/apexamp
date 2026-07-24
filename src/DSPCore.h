#pragma once

#include <vector>

#include "NoiseGate.h"
#include "CabSimulator.h"
#include "Tuner.h"
#include "Metronome.h"
#include <memory>
// Forward declaration for NAM DSP to avoid deep includes
namespace nam { class DSP; }

namespace namapp {
namespace dsp {

class DSPCore {
public:
    DSPCore();
    ~DSPCore();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void processBlock(std::vector<float>& inputBuffer, std::vector<float>& outputBuffer);
    
    void setNoiseGateThreshold(float thresholdDB);
    void loadNamModel(const std::string& modelPath);
    void loadCabIR(const std::string& irPath);

    Tuner& getTuner() { return mTuner; }
    Metronome& getMetronome() { return mMetronome; }

private:
    double mSampleRate{44100.0};
    int mSamplesPerBlock{512};
    float mNoiseGateThreshold{-60.0f};

    NoiseGate mNoiseGate;
    std::unique_ptr<nam::DSP> mNamModel;
    CabSimulator mCabSimulator;
    
    Tuner mTuner;
    Metronome mMetronome;
};

} // namespace dsp
} // namespace namapp
