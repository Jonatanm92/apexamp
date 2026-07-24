#pragma once

#include <vector>
#include <string>

namespace namapp {
namespace dsp {

class Tuner {
public:
    Tuner();
    void prepare(double sampleRate);
    void process(const std::vector<float>& buffer);

    float getCurrentPitchHz() const;
    std::string getClosestNote() const;
    float getCentsOff() const;

private:
    double mSampleRate{44100.0};
    float mCurrentPitchHz{0.0f};
    
    // Internal auto-correlation or YIN algorithm data would go here
    std::vector<float> mCorrelationBuffer;
};

} // namespace dsp
} // namespace namapp
