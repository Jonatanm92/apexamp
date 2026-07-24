#include <jni.h>
#include <oboe/Oboe.h>
#include <memory>
#include "../../src/DSPCore.h"

using namespace namapp::dsp;

class NamAudioEngine : public oboe::AudioStreamCallback {
public:
    NamAudioEngine() {
        mDspCore = std::make_unique<DSPCore>();
        startStream();
    }

    ~NamAudioEngine() {
        if (mStream) {
            mStream->stop();
            mStream->close();
        }
    }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override {
        auto* floatData = static_cast<float*>(audioData);
        // Oboe gives us an interleaved buffer if stereo, but let's assume mono for guitar input.
        
        // Wrap input data into vector for the DSPCore interface (simplification)
        std::vector<float> inputBuffer(floatData, floatData + numFrames);
        std::vector<float> outputBuffer(numFrames, 0.0f);

        mDspCore->processBlock(inputBuffer, outputBuffer);

        // Write output back to Oboe buffer
        for (int i = 0; i < numFrames; ++i) {
            floatData[i] = outputBuffer[i];
        }

        return oboe::DataCallbackResult::Continue;
    }

private:
    void startStream() {
        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::InputOutput)
               ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
               ->setSharingMode(oboe::SharingMode::Exclusive)
               ->setFormat(oboe::AudioFormat::Float)
               ->setChannelCount(1) // Mono in/out
               ->setSampleRate(48000)
               ->setCallback(this);

        oboe::Result result = builder.openStream(mStream);
        if (result == oboe::Result::OK && mStream) {
            mDspCore->prepareToPlay(mStream->getSampleRate(), mStream->getFramesPerBurst());
            mStream->requestStart();
        }
    }

    std::shared_ptr<oboe::AudioStream> mStream;
    std::unique_ptr<DSPCore> mDspCore;
};

// Global instance
std::unique_ptr<NamAudioEngine> gEngine;

extern "C" JNIEXPORT void JNICALL
Java_com_namapp_MainActivity_startEngine(JNIEnv* env, jobject /* this */) {
    if (!gEngine) {
        gEngine = std::make_unique<NamAudioEngine>();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_namapp_MainActivity_stopEngine(JNIEnv* env, jobject /* this */) {
    gEngine.reset();
}
