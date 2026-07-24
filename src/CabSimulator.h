#pragma once

#include <vector>
#include <string>

// Forward declaration for JUCE DSP components to avoid heavy includes in the header
namespace juce {
    namespace dsp {
        class Convolution;
    }
}

namespace namapp {
namespace dsp {

class CabSimulator {
public:
    CabSimulator();
    ~CabSimulator();

    void prepare(double sampleRate, int samplesPerBlock);
    void process(std::vector<float>& buffer);

    bool loadImpulseResponse(const std::string& filePath);
    void setDryWet(float wetLevel);

private:
    // PIMPL idiom to hide JUCE dependencies from this header
    struct Impl;
    Impl* pImpl;
};

} // namespace dsp
} // namespace namapp
