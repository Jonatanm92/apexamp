#pragma once

#include "Biquad.h"
#include <cmath>

namespace apex
{
enum class TonestackModel
{
    marshall = 0,   // "James" stack, midrange presence, slight upper-mid bite
    fender,         // scooped mids, sparkly top, looser lows
    mesa,           // tight lows, deep mid-scoop capability
    modernMetal     // voiced for down-tuned clarity: tight lows + defined high-mids
};

/**
    Bass / Mid / Treble tone stack with selectable amp voicing.

    Rather than convolving a literal RC-network transfer function (which is fragile
    to retune), each model maps the three knobs onto a low shelf, a midrange peak,
    and a high shelf with model-specific centre frequencies and Q, plus a fixed
    "voicing" filter that bakes in the character that distinguishes the amps even
    at noon settings (e.g. Fender's inherent mid scoop, Marshall's upper-mid bump).
*/
class Tonestack
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        recalc();
    }

    void setModel (TonestackModel m) noexcept
    {
        if (m != model) { model = m; recalc(); }
    }

    /** All three in 0..1. */
    void setControls (float bassN, float midN, float trebleN) noexcept
    {
        bass = bassN; mid = midN; treble = trebleN;
        recalc();
    }

    void reset() noexcept
    {
        low.reset(); midBand.reset(); high.reset(); voicing.reset();
    }

    inline float processSample (float x) noexcept
    {
        x = low.processSample (x);
        x = midBand.processSample (x);
        x = high.processSample (x);
        x = voicing.processSample (x);
        return x;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    // Map a 0..1 knob to a +/- dB range around its centre (0.5 -> 0 dB).
    static float knobDb (float n, float range) noexcept
    {
        return (n - 0.5f) * 2.0f * range;
    }

    void recalc() noexcept
    {
        float lowF, midF, midQ, highF;
        float voiceF, voiceQ, voiceDb;
        float bassRange = 12.0f, midRange = 12.0f, trebRange = 12.0f;

        switch (model)
        {
            case TonestackModel::marshall:
                lowF = 90.0f;  midF = 650.0f;  midQ = 0.7f; highF = 3200.0f;
                voiceF = 2200.0f; voiceQ = 0.9f; voiceDb = 3.0f;  // upper-mid bite
                break;
            case TonestackModel::fender:
                lowF = 80.0f;  midF = 400.0f;  midQ = 0.6f; highF = 4000.0f;
                voiceF = 500.0f; voiceQ = 0.8f; voiceDb = -4.0f;  // built-in mid scoop
                break;
            case TonestackModel::mesa:
                lowF = 110.0f; midF = 750.0f;  midQ = 0.9f; highF = 3500.0f;
                voiceF = 450.0f; voiceQ = 1.0f; voiceDb = -3.0f;  // tight, scoopable
                break;
            case TonestackModel::modernMetal:
            default:
                lowF = 130.0f; midF = 900.0f;  midQ = 1.1f; highF = 4500.0f;
                voiceF = 250.0f; voiceQ = 0.9f; voiceDb = -2.0f;  // tight lows
                midRange = 14.0f;
                break;
        }

        low     = Biquad::makeLowShelf  (fs, lowF,  0.7f, knobDb (bass,   bassRange));
        midBand = Biquad::makePeak      (fs, midF,  midQ, knobDb (mid,    midRange));
        high    = Biquad::makeHighShelf (fs, highF, 0.7f, knobDb (treble, trebRange));
        voicing = Biquad::makePeak      (fs, voiceF, voiceQ, voiceDb);
    }

    double fs = 44100.0;
    TonestackModel model = TonestackModel::marshall;
    float bass = 0.5f, mid = 0.5f, treble = 0.5f;

    Biquad low, midBand, high, voicing;
};
} // namespace apex
