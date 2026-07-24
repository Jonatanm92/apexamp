#pragma once

#include <vector>

namespace namapp {
namespace dsp {

class Metronome {
public:
    Metronome();
    void prepare(double sampleRate);
    void process(std::vector<float>& buffer);

    void setTempo(float bpm);
    void setPlaying(bool shouldPlay);

private:
    double mSampleRate{44100.0};
    float mBpm{120.0f};
    bool mIsPlaying{false};
    
    int mSamplesPerBeat{0};
    int mSampleCounter{0};
    
    void calculateTiming();
};

} // namespace dsp
} // namespace namapp
