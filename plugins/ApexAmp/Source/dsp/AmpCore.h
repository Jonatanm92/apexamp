#pragma once

#include "Biquad.h"
#include "NoiseGate.h"
#include "TubeStage.h"
#include "Tonestack.h"
#include "ChugEnhancer.h"
#include "DualChannelPreamp.h"
#include "PowerAmp.h"
#include <cmath>
#include <algorithm>

namespace apex
{
/**
    The complete amp voice as pure C++ (no JUCE dependency).

    Signal flow per mono sample:
        input trim -> input HPF -> Chug enhancer -> dual-channel preamp
                   -> tonestack -> Low Dirt -> power amp (sag) -> output trim

    This object is intended to run at the (possibly oversampled) processing rate.
    The cabinet IR convolution lives in the JUCE engine downstream, because IR
    convolution is linear and does not need oversampling.
*/
struct AmpParams
{
    PreampChannel  channel  = PreampChannel::tight;
    TonestackModel tonestack = TonestackModel::marshall;

    float inputTrimDb = 0.0f;
    float outputTrimDb = 0.0f;

    float gateThresholdDb = -60.0f;   // input noise gate (-80 = off)

    float gain     = 0.5f;   // preamp drive
    float push     = 0.0f;
    float tight    = 0.3f;
    float superCut = 0.0f;
    float bias     = 0.02f;

    float bass = 0.5f, mid = 0.5f, treble = 0.5f;

    float chug = 0.0f;

    float lowDirtDrive = 0.0f, lowDirtMix = 0.0f;

    float sag = 0.3f, powerDrive = 0.3f;
};

class AmpCore
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        gate.prepare (fs);
        inputHP.setCutoff (fs, 30.0);
        inputBiquad = Biquad::makeHighpass (fs, 70.0, 0.707);
        chug.prepare (fs);
        preamp.prepare (fs);
        tonestack.prepare (fs);
        lowDirt.prepare (fs);
        powerAmp.prepare (fs);
        reset();
        applyParams();
    }

    void reset() noexcept
    {
        gate.reset();
        inputHP.reset(); inputBiquad.reset();
        chug.reset(); preamp.reset(); tonestack.reset();
        lowDirt.reset(); powerAmp.reset();
    }

    void setParams (const AmpParams& p) noexcept
    {
        params = p;
        applyParams();
    }

    inline float processSample (float x) noexcept
    {
        x *= inTrim;
        x = gate.processSample (x);     // gate the DI before any gain (clarity!)
        x = inputHP.processSample (x);
        x = inputBiquad.processSample (x);
        x = chug.processSample (x);
        x = preamp.processSample (x);
        x = tonestack.processSample (x);
        x = lowDirt.processSample (x);
        x = powerAmp.processSample (x);
        x *= outTrim;
        return x;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    void applyParams() noexcept
    {
        inTrim  = std::pow (10.0f, params.inputTrimDb  / 20.0f);
        outTrim = std::pow (10.0f, params.outputTrimDb / 20.0f);

        gate.setThreshold (params.gateThresholdDb);

        preamp.setChannel (params.channel);
        preamp.setParams (params.gain, params.push, params.tight,
                          params.superCut, params.bias);

        tonestack.setModel (params.tonestack);
        tonestack.setControls (params.bass, params.mid, params.treble);

        chug.setAmount (params.chug);
        lowDirt.setParams (params.lowDirtDrive, params.lowDirtMix);
        powerAmp.setParams (params.sag, params.powerDrive);
    }

    double fs = 44100.0;
    AmpParams params;
    float inTrim = 1.0f, outTrim = 1.0f;

    DCBlocker inputHP;
    Biquad    inputBiquad;
    NoiseGate         gate;
    ChugEnhancer      chug;
    DualChannelPreamp preamp;
    Tonestack         tonestack;
    LowDirt           lowDirt;
    PowerAmp          powerAmp;
};
} // namespace apex
