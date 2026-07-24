#include "DSPCore.h"
#include <NAM/dsp.h>
#include <NAM/get_dsp.h>
#include <filesystem>

namespace namapp {
namespace dsp {

DSPCore::DSPCore() {}

DSPCore::~DSPCore() {}

void DSPCore::prepareToPlay(double sampleRate, int samplesPerBlock) {
    mSampleRate = sampleRate;
    mSamplesPerBlock = samplesPerBlock;
    
    mNoiseGate.prepare(sampleRate, samplesPerBlock);
    mCabSimulator.prepare(sampleRate, samplesPerBlock);
    mTuner.prepare(sampleRate);
    mMetronome.prepare(sampleRate);
}

void DSPCore::processBlock(std::vector<float>& inputBuffer, std::vector<float>& outputBuffer) {
    // 0. Metronome (add click before processing or to output, we do it at input for fun/mock)
    mMetronome.process(inputBuffer);
    
    // 1. Tuner (analyze raw input)
    mTuner.process(inputBuffer);
    
    // 2. Noise Gate
    mNoiseGate.process(inputBuffer);
    
    // 3. NAM Amp (A2/A1)
    if (mNamModel) {
        std::vector<double> doubleInput(inputBuffer.begin(), inputBuffer.end());
        std::vector<double> doubleOutput(inputBuffer.size());
        double* inputPtr = doubleInput.data();
        double* outputPtr = doubleOutput.data();
        
        mNamModel->process(&inputPtr, &outputPtr, inputBuffer.size());
        
        for (size_t i = 0; i < inputBuffer.size(); ++i) {
            outputBuffer[i] = static_cast<float>(doubleOutput[i]);
        }
    } else {
        // Pass-through if no model loaded
        if (inputBuffer.size() == outputBuffer.size()) {
            for (size_t i = 0; i < inputBuffer.size(); ++i) {
                outputBuffer[i] = inputBuffer[i];
            }
        }
    }
    
    // 4. Cab IR loader (processes outputBuffer in-place usually)
    mCabSimulator.process(outputBuffer); 
}

void DSPCore::setNoiseGateThreshold(float thresholdDB) {
    mNoiseGateThreshold = thresholdDB;
    // mNoiseGate.setThreshold(thresholdDB); // Assuming NoiseGate has this
}

void DSPCore::loadNamModel(const std::string& modelPath) {
    try {
        mNamModel = nam::get_dsp(std::filesystem::path(modelPath));
    } catch (...) {
        // Handle error loading model
    }
}

void DSPCore::loadCabIR(const std::string& irPath) {
    mCabSimulator.loadImpulseResponse(irPath);
}

} // namespace dsp
} // namespace namapp
