#include "Metronome.h"

namespace namapp {
namespace dsp {

Metronome::Metronome() {
    calculateTiming();
}

void Metronome::prepare(double sampleRate) {
    mSampleRate = sampleRate;
    calculateTiming();
}

void Metronome::setTempo(float bpm) {
    mBpm = bpm;
    calculateTiming();
}

void Metronome::setPlaying(bool shouldPlay) {
    mIsPlaying = shouldPlay;
    if (mIsPlaying) mSampleCounter = 0; // Reset on start
}

void Metronome::calculateTiming() {
    if (mBpm > 0.0f && mSampleRate > 0.0) {
        mSamplesPerBeat = static_cast<int>((60.0 / mBpm) * mSampleRate);
    }
}

void Metronome::process(std::vector<float>& buffer) {
    if (!mIsPlaying || mSamplesPerBeat == 0) return;

    for (size_t i = 0; i < buffer.size(); ++i) {
        if (mSampleCounter == 0) {
            // Play click (mock: just a short burst of noise or impulse)
            buffer[i] += 0.8f; // Add click to the buffer
        }
        
        mSampleCounter++;
        if (mSampleCounter >= mSamplesPerBeat) {
            mSampleCounter = 0;
        }
    }
}

} // namespace dsp
} // namespace namapp
