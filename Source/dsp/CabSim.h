#pragma once

#include "Biquad.h"

namespace apex
{
enum class CabType
{
    modernV30 = 0,   // bright, aggressive, modern metal 4x12
    vintageGreenback, // warmer, darker, classic rock
    tight4x12,        // tight lows, scooped, djent-friendly
    americanScooped   // scooped mids, sparkly top (US voicing)
};

/**
    Filter-based guitar speaker / cabinet emulation (default, no IR required).

    A real impulse response is the gold standard, but a *synthesized noise* IR
    sounds phasey and harsh. This deterministic EQ voicing instead models the
    gross frequency response of a guitar cab, selectable between a few speaker
    characters. It is smooth, predictable, and CPU-cheap. Loading a user IR
    bypasses this voicing in favour of convolution.

    Each voicing is built from: high-pass + low-mid thump + boxiness scoop +
    presence + an "edge"/gnarl shelf + a steep speaker roll-off.
*/
class CabSim
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        build();
        reset();
    }

    void setType (CabType t) noexcept
    {
        if (t != type) { type = t; build(); }
    }

    void reset() noexcept
    {
        hp.reset(); thump.reset(); scoop.reset();
        presence.reset(); edge.reset(); lp1.reset(); lp2.reset();
    }

    inline float processSample (float x) noexcept
    {
        x = hp.processSample (x);
        x = thump.processSample (x);
        x = scoop.processSample (x);
        x = presence.processSample (x);
        x = edge.processSample (x);
        x = lp1.processSample (x);
        x = lp2.processSample (x);
        return x;
    }

    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

private:
    void build() noexcept
    {
        // Per-voicing parameters.
        double hpF, thumpF, thumpDb, scoopF, scoopDb, presF, presDb, edgeF, edgeDb, lpF;

        switch (type)
        {
            case CabType::vintageGreenback:
                hpF = 90.0;  thumpF = 130.0; thumpDb = 2.5; scoopF = 500.0; scoopDb = -1.5;
                presF = 2400.0; presDb = 3.0; edgeF = 4000.0; edgeDb = 0.5; lpF = 4800.0; // darker
                break;
            case CabType::tight4x12:
                hpF = 105.0; thumpF = 150.0; thumpDb = 1.5; scoopF = 500.0; scoopDb = -4.0;
                presF = 3600.0; presDb = 5.0; edgeF = 4800.0; edgeDb = 3.0; lpF = 6500.0; // tight + aggressive
                break;
            case CabType::americanScooped:
                hpF = 80.0;  thumpF = 95.0;  thumpDb = 1.5; scoopF = 550.0; scoopDb = -5.0;
                presF = 4000.0; presDb = 4.0; edgeF = 5000.0; edgeDb = 3.5; lpF = 7000.0; // scooped + sparkly
                break;
            case CabType::modernV30:
            default:
                hpF = 85.0;  thumpF = 100.0; thumpDb = 1.0; scoopF = 450.0; scoopDb = -2.5;
                presF = 3500.0; presDb = 5.0; edgeF = 4500.0; edgeDb = 3.0; lpF = 6500.0; // bright modern
                break;
        }

        hp       = Biquad::makeHighpass  (fs, hpF,    0.8);
        thump    = Biquad::makePeak      (fs, thumpF, 1.0,  (float) thumpDb);
        scoop    = Biquad::makePeak      (fs, scoopF, 1.0,  (float) scoopDb);
        presence = Biquad::makePeak      (fs, presF,  1.2,  (float) presDb);
        edge     = Biquad::makeHighShelf (fs, edgeF,  0.7,  (float) edgeDb);
        lp1      = Biquad::makeLowpass   (fs, lpF,        0.707);
        lp2      = Biquad::makeLowpass   (fs, lpF + 300.0, 0.707); // cascade => steeper
    }

    double fs = 44100.0;
    CabType type = CabType::modernV30;
    Biquad hp, thump, scoop, presence, edge, lp1, lp2;
};
} // namespace apex
