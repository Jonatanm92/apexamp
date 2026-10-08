#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cmath>

#include "NAM/get_dsp.h"
#include "BinaryData.h"
#include "apex/dsp/AbyssReverb.h"
#include "apex/dsp/ChugShaper.h"
#include "apex/dsp/LowDirt.h"
#include "apex/dsp/PitchShifter.h"
#include "apex/dsp/StereoDelay.h"

/**
 * NamEngine
 * ---------
 * The heart of the NAM-powered ApexAmp. It owns three Neural Amp Modeler rigs
 * (the user's own captures) and three cabinet impulse responses, all embedded
 * in the binary. It can either:
 *   - SINGLE mode: run one rig on its own, or
 *   - BLEND mode : sum all three loudness-matched rigs (layered tone).
 * After the amp stage the signal passes through the amp EQ (Bass / Mid /
 * Treble, flat at 0 dB), the Shape pedal (Chug + Low Dirt) and the selected
 * cabinet IR, then the stereo Void unit (Echo delay and Abyss reverb, both
 * with spillover).
 *
 * In front of the amp sits the Drop pedal: apex-dsp's Live pitch engine
 * (formant-correct "Body") plus an optional sub-octave layer.
 *
 * All NAM I/O is double precision (NAM_SAMPLE == double); JUCE buffers are
 * float, so we convert at the boundary. NAM rigs were trained at 48 kHz; for
 * milestone 1 we Reset() at the host sample rate (slight voicing shift off
 * 48 k; real resampling is a TODO before release).
 */
class NamEngine
{
public:
    enum class RigMode { Blend = 0, Single = 1 };

    struct Params
    {
        float   inputGainDb  = 0.0f;
        float   inputTrimDb  = 0.0f;       // Auto Input calibration
        float   outputGainDb = 0.0f;
        float   tightHz      = 20.0f;      // pre-amp high-pass (20 = off)
        bool    boostOn      = false;      // Screamer boost in front of the amp
        float   boostDrive   = 0.5f;       // 0..1
        float   boostTone    = 0.5f;       // 0..1 (dark..bright)
        float   boostLevelDb = 0.0f;       // pedal output level
        RigMode rigMode      = RigMode::Single;
        int     singleIndex  = 0;          // 0..2
        std::array<float, 3> mix { 1.0f, 1.0f, 1.0f }; // blend layer mixes 0..1
        int     irIndex      = 0;          // 0..2
        float   cabMix       = 1.0f;       // 0 = dry amp, 1 = full cab (smoothed)
        bool    gateEnabled  = false;
        float   gateThreshDb = -60.0f;     // open above this
        float   gateHoldMs   = 50.0f;      // hold open after last transient
        float   lowCutHz     = 80.0f;      // output high-pass to tame sub-bass
        float   presenceDb   = 0.0f;       // high shelf post-cab (-12..+12 dB)
        bool    dropOn       = false;      // Drop pedal (pitch shift in front of the amp)
        float   dropShift    = -5.0f;      // semitones
        float   dropBody     = 1.0f;       // 0..1 formant preservation
        float   dropSub      = 0.0f;       // 0..1 sub-octave layer level
        float   bassDb       = 0.0f;       // amp EQ, flat at 0 dB
        float   midDb        = 0.0f;
        float   trebleDb     = 0.0f;
        float   depthDb      = 0.0f;       // low resonance after the amp, flat at 0 dB
        float   highCutHz    = 20000.0f;   // post-cab low-pass (20 kHz = off)
        bool    shapeOn      = false;      // Shape pedal: Chug + Low Dirt
        float   chug         = 0.5f;       // 0..1
        float   chugFreqHz   = 700.0f;
        float   dirt         = 0.25f;      // 0..1
        bool    delayOn      = false;      // Void: Echo
        double  delayMs      = 375.0;
        float   delayFeedback = 0.35f;     // 0..1
        float   delayDuck    = 0.0f;       // 0..1
        float   delayMix     = 0.3f;       // 0..1
        bool    reverbOn     = false;      // Void: Abyss
        float   reverbDecay  = 3.5f;       // seconds
        float   reverbAbyss  = 0.25f;      // 0..1
        float   reverbTone   = 0.5f;       // 0..1
        float   reverbMix    = 0.25f;      // 0..1
    };

    NamEngine() = default;

    //==============================================================================
    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        this->sampleRate = sampleRate;
        this->maxBlock   = maxBlockSize;

        // NAM rigs are 48 kHz captures. At any other host rate we resample the
        // amp stage to/from 48 kHz so the model always runs at its native rate
        // (correct voicing). At exactly 48 kHz this is a pure bypass.
        resampling  = std::abs (sampleRate - 48000.0) > 1.0;
        namRate     = resampling ? 48000.0 : sampleRate;
        namMaxBlock = resampling
            ? (int) std::ceil ((maxBlockSize + 4) * 48000.0 / sampleRate) + 16
            : maxBlockSize;

        loadRigs();

        for (auto& rig : rigs)
            if (rig)
                rig->Reset (namRate, namMaxBlock);

        // Scratch buffers. Host-rate buffers use maxBlock; the 48 kHz NAM
        // buffers use namMaxBlock (which can exceed the host block size).
        monoIn.assign    ((size_t) maxBlockSize, 0.0);
        mixBuf.assign    ((size_t) maxBlockSize, 0.0);
        rigOut.assign    ((size_t) namMaxBlock, 0.0);
        nam48.assign     ((size_t) namMaxBlock, 0.0);
        nam48out.assign  ((size_t) namMaxBlock, 0.0);
        monoInF.assign   ((size_t) maxBlockSize, 0.0f);
        nam48fOut.assign ((size_t) namMaxBlock, 0.0f);

        if (resampling)
        {
            srcUp.prepare   (sampleRate, 48000.0);
            srcDown.prepare (48000.0, sampleRate);
            up48f.clear();
            up48f.reserve ((size_t) namMaxBlock + 16);
            hostOutFifo.assign ((size_t) kPrime, 0.0f);
            hostOutFifo.reserve ((size_t) (maxBlockSize + kPrime + 64));
            latencySamples = kPrime;
        }
        else
        {
            latencySamples = 0;
        }

        // Cabinet convolution: one convolver per IR so switching is click-free
        // and we never reload on the audio thread.
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = (juce::uint32) maxBlockSize;
        spec.numChannels      = (juce::uint32) juce::jmax (1, numChannels);

        loadConvolvers (spec);
        lastSpec = spec;

        // User cabinet IR slot (loaded on demand from a file).
        userConvolver.prepare (spec);
        if (userIrFile.existsAsFile())
            loadUserIr (userIrFile);

        // User NAM rig slot: re-Reset existing, or (re)load from file.
        if (userRig)
            userRig->Reset (namRate, namMaxBlock);
        else if (userRigFile.existsAsFile())
            loadUserRig (userRigFile);

        // Output low-cut (high-pass) to remove the sub-bass rumble a cranked
        // high-gain amp produces. Default ~80 Hz keeps weight without flub.
        lowCut.prepare (spec);
        lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        lowCut.setResonance (0.707f);
        lowCut.setCutoffFrequency (80.0f);

        // Presence: high-shelf at 3.5 kHz, lets you add/cut brightness
        // through the cab without going fully dry.
        presenceFilter.prepare (spec);
        updatePresenceCoeffs (0.0f);

        highCut.prepare (spec);
        highCut.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        highCut.setResonance (0.707f);
        highCut.setCutoffFrequency (20000.0f);

        // Dry/wet crossfade state for the cab.
        dryScratch.assign ((size_t) juce::jmax (1, numChannels),
                           std::vector<float> ((size_t) maxBlockSize, 0.0f));
        cabMixSmoothed.reset (sampleRate, 0.02); // 20 ms ramp = click-free
        cabMixSmoothed.setCurrentAndTargetValue (1.0f);

        // Drop pedal: main shifter + sub-octave layer (Live engine, mono).
        dropMain.prepare (sampleRate, maxBlockSize, 1);
        dropSubShifter.prepare (sampleRate, maxBlockSize, 1);
        dropMain.setMode (apex::dsp::PitchMode::live);
        dropSubShifter.setMode (apex::dsp::PitchMode::live);
        dropIn.assign  ((size_t) maxBlockSize, 0.0f);
        dropDry.assign ((size_t) maxBlockSize, 0.0f);
        dropSubBuf.assign ((size_t) maxBlockSize, 0.0f);
        dropGain.reset (sampleRate, 0.012);
        dropGain.setCurrentAndTargetValue (0.0f);
        subLevel.reset (sampleRate, 0.03);
        subLevel.setCurrentAndTargetValue (0.0f);
        dropWasActive = false;
        subLowpass.setLowpass (sampleRate, 240.0);
        subLowpass.reset();

        for (auto& band : ampEq) band.reset();
        for (auto& last : lastEq) last = 1.0e9f;

        // Shape pedal and the Void unit
        chugShaper.prepare (sampleRate, maxBlockSize);
        chugShaper.setDetectorDelay (latencySamples);
        lowDirt.prepare (sampleRate);
        shapeDi.assign ((size_t) maxBlockSize, 0.0f);
        shapeBuf.assign ((size_t) maxBlockSize, 0.0f);
        echo.prepare (sampleRate, maxBlockSize);
        abyss.prepare (sampleRate, maxBlockSize);

        // Gate / filter state
        gateGain = 1.0f;
        gateOpen = true;
        gateHoldCounter = 0;
        tightLp1 = tightLp2 = 0.0;
        screamer.reset();

        prepared = true;
    }

    void reset()
    {
        for (auto& rig : rigs)
            if (rig)
                rig->Reset (namRate, namMaxBlock);
        for (auto& c : convolvers)
            c.reset();
        userConvolver.reset();
        {
            const juce::SpinLock::ScopedLockType sl (userRigLock);
            if (userRig)
                userRig->Reset (namRate, namMaxBlock);
        }
        lowCut.reset();
        presenceFilter.reset();
        highCut.reset();
        srcUp.reset();
        srcDown.reset();
        if (resampling)
            hostOutFifo.assign ((size_t) kPrime, 0.0f);
        gateGain = 1.0f;
        gateOpen = true;
        gateHoldCounter = 0;
        tightLp1 = tightLp2 = 0.0;
        screamer.reset();
        dropMain.reset();
        dropSubShifter.reset();
        subLowpass.reset();
        for (auto& band : ampEq) band.reset();
        chugShaper.reset();
        lowDirt.reset();
        echo.reset();
        abyss.reset();
    }

    int getLatencySamples() const noexcept { return latencySamples; }

    /** Extra latency while the Drop pedal is on (Live engine, reported to the host). */
    int getDropLatencySamples() const noexcept { return dropMain.getLatencySamples(); }

    //==============================================================================
    // Load a user cabinet IR from a file (message thread). Returns true on success.
    bool loadUserIr (const juce::File& file)
    {
        if (! file.existsAsFile())
            return false;

        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0)
            return false;

        // Measure makeup (same broadband method as the built-in cabs).
        const int chs = (int) reader->numChannels;
        const int len = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> irBuf (juce::jmax (1, chs), len);
        reader->read (&irBuf, 0, len, 0, true, true);

        std::vector<float> irMono ((size_t) len, 0.0f);
        for (int ch = 0; ch < irBuf.getNumChannels(); ++ch)
        {
            const float* d = irBuf.getReadPointer (ch);
            for (int k = 0; k < len; ++k) irMono[(size_t) k] += d[k];
        }
        const float inv = 1.0f / (float) juce::jmax (1, irBuf.getNumChannels());
        for (auto& v : irMono) v *= inv;

        double makeup = 1.0;
        if (! ampRefStored.empty() && ampRefRmsStored > 1.0e-9)
        {
            const double wet = rmsOfConvolution (ampRefStored, irMono);
            if (wet > 1.0e-9) makeup = ampRefRmsStored / wet;
        }
        userCabMakeup.store ((float) juce::jlimit (0.0625, 64.0, makeup));

        userConvolver.loadImpulseResponse (
            file, juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::yes, 0,
            juce::dsp::Convolution::Normalise::no);

        userIrFile = file;
        userIrLoaded.store (true);
        return true;
    }

    // Load a user NAM rig (.nam) from a file (message thread). RT-safe swap.
    bool loadUserRig (const juce::File& file)
    {
        if (! file.existsAsFile())
            return false;

        std::unique_ptr<nam::DSP> dsp;
        try
        {
            dsp = nam::get_dsp (std::filesystem::path (file.getFullPathName().toStdString()));
        }
        catch (const std::exception&) { return false; }
        if (! dsp) return false;

        dsp->Reset (namRate, namMaxBlock);
        const double loud  = dsp->HasLoudness() ? dsp->GetLoudness() : kTargetLufs;
        const double match = std::pow (10.0, (kTargetLufs - loud) / 20.0);

        {
            const juce::SpinLock::ScopedLockType sl (userRigLock);
            userRig      = std::move (dsp);
            userRigMatch = match;
        }
        userRigFile = file;
        userRigReady.store (true);
        return true;
    }

    bool hasUserIr()  const noexcept { return userIrLoaded.load(); }
    bool hasUserRig() const noexcept { return userRigReady.load(); }

    //==============================================================================
    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        if (! prepared)
            return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels  = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0)
            return;

        const int n = juce::jmin (numSamples, maxBlock);

        const double inGain  = juce::Decibels::decibelsToGain ((double) (p.inputGainDb + p.inputTrimDb));
        const double outGain = juce::Decibels::decibelsToGain ((double) p.outputGainDb);

        // ---- 1) Sum to mono double, apply input gain + optional gate -----------
        const float gateThresh = juce::Decibels::decibelsToGain (p.gateThreshDb);
        const float gateClose = gateThresh * 0.5f; // hysteresis: closes 6 dB below open
        const float gateAtk  = std::exp (-1.0f / (0.0005f * (float) sampleRate)); // 0.5 ms attack (fast open)
        const float gateRel  = std::exp (-1.0f / (0.050f * (float) sampleRate));  // 50 ms release (smooth close)
        const int   holdSamp = (int) (p.gateHoldMs * 0.001f * (float) sampleRate);

        // Tightness: cascaded one-pole high-pass on the DI feeding the amp.
        const bool   tightOn    = p.tightHz > 21.0f;
        const double tightAlpha = tightOn
            ? 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * (double) p.tightHz / sampleRate)
            : 0.0;

        for (int i = 0; i < n; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                sum += buffer.getReadPointer (ch)[i];
            dropIn[(size_t) i] = sum / (float) numChannels * (float) inGain;
        }

        processDrop (p, n);

        for (int i = 0; i < n; ++i)
        {
            double sum = (double) dropIn[(size_t) i];

            if (tightOn)
            {
                tightLp1 += tightAlpha * (sum - tightLp1);
                const double h1 = sum - tightLp1;
                tightLp2 += tightAlpha * (h1 - tightLp2);
                sum = h1 - tightLp2;
            }

            if (p.gateEnabled)
            {
                const float level = std::abs ((float) sum);
                // Hysteresis: open at threshold, close at threshold-6dB
                if (level > gateThresh)
                {
                    gateOpen = true;
                    gateHoldCounter = holdSamp;
                }
                else if (level < gateClose && gateHoldCounter <= 0)
                {
                    gateOpen = false;
                }

                if (gateHoldCounter > 0)
                    --gateHoldCounter;

                const float target = gateOpen ? 1.0f : 0.0f;
                const float coef   = (target > gateGain) ? gateAtk : gateRel;
                gateGain = target + coef * (gateGain - target);
                sum *= (double) gateGain;
            }

            monoIn[(size_t) i] = sum;
        }

        // ---- 1.4) Chug detector listens to the DI the amp is about to get ---------
        for (int i = 0; i < n; ++i)
            shapeDi[(size_t) i] = (float) monoIn[(size_t) i];
        chugShaper.analyse (shapeDi.data(), n);

        // ---- 1.5) Screamer boost in front of the amp (optional) ----------------
        if (p.boostOn)
            screamer.process (monoIn.data(), n, sampleRate,
                              p.boostDrive, p.boostTone,
                              juce::Decibels::decibelsToGain ((double) p.boostLevelDb));

        // ---- 2) Run NAM rig(s) -> mixBuf (at 48 kHz when host rate differs) ----
        if (! resampling)
        {
            runRigs (p, monoIn.data(), mixBuf.data(), n);
        }
        else
        {
            for (int i = 0; i < n; ++i)
                monoInF[(size_t) i] = (float) monoIn[(size_t) i];

            // host -> 48 kHz
            up48f.clear();
            srcUp.process (monoInF.data(), n, up48f);
            const int k = juce::jmin ((int) up48f.size(), namMaxBlock);

            for (int i = 0; i < k; ++i)
                nam48[(size_t) i] = (double) up48f[(size_t) i];

            runRigs (p, nam48.data(), nam48out.data(), k);

            for (int i = 0; i < k; ++i)
                nam48fOut[(size_t) i] = (float) nam48out[(size_t) i];

            // 48 kHz -> host, buffered so we always emit exactly n samples
            srcDown.process (nam48fOut.data(), k, hostOutFifo);

            const int avail = (int) hostOutFifo.size();
            const int take  = juce::jmin (avail, n);
            for (int i = 0; i < take; ++i)
                mixBuf[(size_t) i] = (double) hostOutFifo[(size_t) i];
            for (int i = take; i < n; ++i)
                mixBuf[(size_t) i] = 0.0;
            if (take > 0)
                hostOutFifo.erase (hostOutFifo.begin(), hostOutFifo.begin() + take);
        }

        // ---- 2.5) Amp EQ (Bass / Mid / Treble; exactly flat at 0 dB) ----------
        updateEqCoeffs (p);
        for (auto& band : ampEq)
            band.process (mixBuf.data(), n);

        // ---- 2.6) Shape pedal: Chug (dynamic punch) + Low Dirt ------------------
        // Amounts glide to zero when the pedal is off; at zero both are exact
        // bypass, so presets without Shape sound exactly as before.
        chugShaper.setAmount (p.shapeOn ? p.chug : 0.0f);
        chugShaper.setFrequency (p.chugFreqHz);
        lowDirt.setAmount (p.shapeOn ? p.dirt : 0.0f);
        for (int i = 0; i < n; ++i)
            shapeBuf[(size_t) i] = (float) mixBuf[(size_t) i];
        chugShaper.process (shapeBuf.data(), n);
        lowDirt.process (shapeBuf.data(), n);

        // ---- 3) Write mono result back to all channels (float) -----------------
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            std::copy (shapeBuf.begin(), shapeBuf.begin() + n, w);
        }

        // ---- 4) Cabinet IR convolution with smooth dry/wet blend ---------------
        // The convolver always runs (cheap for short IRs); we crossfade dry amp
        // vs cabbed signal with a smoothed Cab Mix so changing the cab amount
        // never clicks or jumps in level ("cab status weirdness").
        {
            const int idx = juce::jlimit (0, 3, p.irIndex);

            // Pick convolver + makeup: user slot if selected and loaded.
            juce::dsp::Convolution* conv = nullptr;
            float mk = 1.0f;
            if (idx == 3 && userIrLoaded.load())
            {
                conv = &userConvolver;
                mk   = userCabMakeup.load();
            }
            else
            {
                const int bi = (idx == 3 ? 0 : idx);
                conv = &convolvers[(size_t) bi];
                mk   = cabMakeup[(size_t) bi];
            }

            // Stash the dry (pre-cab) amp signal.
            for (int ch = 0; ch < numChannels; ++ch)
                std::copy (buffer.getReadPointer (ch),
                           buffer.getReadPointer (ch) + n,
                           dryScratch[(size_t) ch].data());

            // wet = makeup * cab(buffer)
            {
                juce::dsp::AudioBlock<float>              block (buffer);
                juce::dsp::ProcessContextReplacing<float> ctx (block);
                conv->process (ctx);
            }
            buffer.applyGain (mk);

            // Smoothed crossfade: out = dry*(1-m) + wet*m.
            cabMixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.cabMix));
            for (int i = 0; i < n; ++i)
            {
                const float m = cabMixSmoothed.getNextValue();
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto* w = buffer.getWritePointer (ch);
                    w[i] = dryScratch[(size_t) ch][(size_t) i] * (1.0f - m) + w[i] * m;
                }
            }
        }

        // ---- 5) Presence shelf (post-cab brightness control) --------------------
        if (std::abs (p.presenceDb) > 0.05f)
        {
            updatePresenceCoeffs (p.presenceDb);
            juce::dsp::AudioBlock<float>          block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            presenceFilter.process (ctx);
        }

        // ---- 5.5) High cut (post-cab low-pass, off at 20 kHz) ------------------
        if (p.highCutHz < 19500.0f)
        {
            highCut.setCutoffFrequency (juce::jlimit (1000.0f, (float) (0.45 * sampleRate), p.highCutHz));
            juce::dsp::AudioBlock<float>          block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            highCut.process (ctx);
            highCutActive = true;
        }
        else if (highCutActive)
        {
            highCut.reset();   // switched off: the signal passes untouched
            highCutActive = false;
        }

        // ---- 6) Output low-cut (high-pass) -------------------------------------
        lowCut.setCutoffFrequency (juce::jlimit (20.0f, 300.0f, p.lowCutHz));
        {
            juce::dsp::AudioBlock<float>          block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            lowCut.process (ctx);
        }

        // ---- 7) Void: Echo -> Abyss, stereo, with spillover ---------------------
        {
            float* left  = buffer.getWritePointer (0);
            float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

            echo.setEnabled (p.delayOn);
            echo.setTime (p.delayMs);
            echo.setFeedback (p.delayFeedback);
            echo.setDuck (p.delayDuck);
            echo.setMix (p.delayMix);
            echo.process (left, right, n);

            abyss.setEnabled (p.reverbOn);
            abyss.setDecay (p.reverbDecay);
            abyss.setAbyss (p.reverbAbyss);
            abyss.setTone (p.reverbTone);
            abyss.setMix (p.reverbMix);
            abyss.process (left, right, n);

            for (int ch = 2; ch < numChannels; ++ch)
                std::copy (left, left + n, buffer.getWritePointer (ch));
        }

        // ---- 8) Output gain -----------------------------------------------------
        buffer.applyGain ((float) outGain);
    }

    bool isReady() const noexcept { return prepared; }

    /** Chug punch envelope (0..1) of the last block, for the editor. */
    float getChugPunch() const noexcept { return chugShaper.getPunch(); }
    double getLoudness (int idx) const noexcept { return loudness[(size_t) juce::jlimit (0, 2, idx)]; }

private:
    //==============================================================================
    // Runs the selected rig (Single) or all three loudness-matched rigs (Blend)
    // over `count` samples of `in`, writing the result to `out`. Uses rigOut as
    // per-rig scratch (sized to namMaxBlock).
    void runRigs (const Params& p, const double* in, double* out, int count)
    {
        std::fill (out, out + count, 0.0);

        if (p.rigMode == RigMode::Single)
        {
            const int idx = juce::jlimit (0, 3, p.singleIndex);

            if (idx == 3) // user rig
            {
                const juce::SpinLock::ScopedTryLockType sl (userRigLock);
                if (sl.isLocked() && userRig)
                {
                    std::copy (in, in + count, rigOut.begin());
                    double* ip[1] = { rigOut.data() };
                    double* op[1] = { rigOut.data() };
                    userRig->process (ip, op, count);
                    for (int i = 0; i < count; ++i)
                        out[i] = rigOut[(size_t) i] * userRigMatch;
                }
                else
                {
                    std::copy (in, in + count, out); // not ready -> dry passthrough
                }
            }
            else if (rigs[(size_t) idx])
            {
                std::copy (in, in + count, rigOut.begin());
                double* ip[1] = { rigOut.data() };
                double* op[1] = { rigOut.data() };
                rigs[(size_t) idx]->process (ip, op, count);
                const double g = matchGain[(size_t) idx];
                for (int i = 0; i < count; ++i)
                    out[i] = rigOut[(size_t) i] * g;
            }
        }
        else // Blend
        {
            for (size_t r = 0; r < rigs.size(); ++r)
            {
                if (! rigs[r]) continue;
                std::copy (in, in + count, rigOut.begin());
                double* ip[1] = { rigOut.data() };
                double* op[1] = { rigOut.data() };
                rigs[r]->process (ip, op, count);
                const double g = matchGain[r] * (double) juce::jlimit (0.0f, 1.0f, p.mix[r]) / 3.0;
                for (int i = 0; i < count; ++i)
                    out[i] += rigOut[(size_t) i] * g;
            }
        }
    }

    //==============================================================================
    // Streaming arbitrary-ratio resampler (Lagrange) with an input FIFO so we
    // can convert variable-length blocks while preserving phase continuity.
    struct RateConverter
    {
        juce::LagrangeInterpolator interp;
        double ratio = 1.0;           // input samples consumed per output sample
        std::vector<float> fifo;

        void prepare (double srcRate, double dstRate)
        {
            interp.reset();
            ratio = srcRate / dstRate;
            fifo.clear();
            fifo.reserve (8192);
        }

        void reset()
        {
            interp.reset();
            fifo.clear();
        }

        // Appends `numSrc` input samples, then appends as many resampled output
        // samples as are currently available to `out`.
        void process (const float* src, int numSrc, std::vector<float>& out)
        {
            fifo.insert (fifo.end(), src, src + numSrc);
            const int avail = (int) fifo.size();
            const int numOut = (int) std::floor ((double) (avail - 2) / ratio);
            if (numOut <= 0)
                return;

            const size_t base = out.size();
            out.resize (base + (size_t) numOut);
            const int used = interp.process (ratio, fifo.data(), out.data() + base, numOut);
            const int toErase = juce::jlimit (0, (int) fifo.size(), used);
            if (toErase > 0)
                fifo.erase (fifo.begin(), fifo.begin() + toErase);
        }
    };

    static constexpr int kPrime = 32; // output FIFO priming = resampler latency

    //==============================================================================
    // RBJ biquad, transposed direct form II, double precision (mono).
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0.0; }

        void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
        {
            b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
        }

        void setLowpass (double sr, double f)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * 0.7071);
            set ((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
        }

        void setShelf (double sr, double f, double db, bool high)
        {
            const double A = std::pow (10.0, db / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / sr;
            const double c = std::cos (w), al = std::sin (w) / 2.0 * std::sqrt (2.0), sa = 2.0 * std::sqrt (A) * al;
            if (high)
                set (A * ((A + 1.0) + (A - 1.0) * c + sa), -2.0 * A * ((A - 1.0) + (A + 1.0) * c), A * ((A + 1.0) + (A - 1.0) * c - sa),
                     (A + 1.0) - (A - 1.0) * c + sa, 2.0 * ((A - 1.0) - (A + 1.0) * c), (A + 1.0) - (A - 1.0) * c - sa);
            else
                set (A * ((A + 1.0) - (A - 1.0) * c + sa), 2.0 * A * ((A - 1.0) - (A + 1.0) * c), A * ((A + 1.0) - (A - 1.0) * c - sa),
                     (A + 1.0) + (A - 1.0) * c + sa, -2.0 * ((A - 1.0) + (A + 1.0) * c), (A + 1.0) + (A - 1.0) * c - sa);
        }

        void setPeak (double sr, double f, double q, double db)
        {
            const double A = std::pow (10.0, db / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / sr;
            const double c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set (1.0 + al * A, -2.0 * c, 1.0 - al * A, 1.0 + al / A, -2.0 * c, 1.0 - al / A);
        }

        void process (double* x, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                const double y = b0 * x[i] + z1;
                z1 = b1 * x[i] - a1 * y + z2;
                z2 = b2 * x[i] - a2 * y;
                x[i] = y;
            }
        }

        float processSample (float x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
    };

    void updateEqCoeffs (const Params& p)
    {
        const float gains[4] = { p.bassDb, p.midDb, p.trebleDb, p.depthDb };
        for (int b = 0; b < 4; ++b)
        {
            if (std::abs (gains[b] - lastEq[b]) < 0.005f)
                continue;
            lastEq[b] = gains[b];
            if (b == 0)      ampEq[0].setShelf (sampleRate, 110.0, gains[0], false);
            else if (b == 1) ampEq[1].setPeak (sampleRate, 700.0, 0.8, gains[1]);
            else if (b == 2) ampEq[2].setShelf (sampleRate, 2600.0, gains[2], true);
            else             ampEq[3].setPeak (sampleRate, 85.0, 1.1, gains[3]);   // power-amp style resonance
        }
    }

    // Drop pedal on the mono, gain-staged DI in dropIn[0..n).
    void processDrop (const Params& p, int n)
    {
        dropGain.setTargetValue (p.dropOn ? 1.0f : 0.0f);
        const bool active = p.dropOn || dropGain.isSmoothing();
        if (! active)
        {
            dropWasActive = false;
            return;
        }
        if (! dropWasActive)
        {
            // Fresh buffers: no stale audio from the last time the pedal was on.
            dropMain.reset();
            dropSubShifter.reset();
            subLowpass.reset();
            dropWasActive = true;
        }

        std::copy (dropIn.begin(), dropIn.begin() + n, dropDry.begin());

        dropMain.setSemitones (p.dropShift);
        dropMain.setBody (p.dropBody);
        float* mainPtr[1] = { dropIn.data() };
        dropMain.process (mainPtr, 1, n);

        subLevel.setTargetValue (p.dropSub);
        if (p.dropSub > 0.0f || subLevel.isSmoothing())
        {
            std::copy (dropDry.begin(), dropDry.begin() + n, dropSubBuf.begin());
            dropSubShifter.setSemitones (p.dropShift - 12.0f);
            dropSubShifter.setBody (p.dropBody);
            float* subPtr[1] = { dropSubBuf.data() };
            dropSubShifter.process (subPtr, 1, n);
            for (int i = 0; i < n; ++i)
                dropIn[(size_t) i] += subLowpass.processSample (dropSubBuf[(size_t) i]) * subLevel.getNextValue() * 1.4f;
        }

        for (int i = 0; i < n; ++i)
        {
            const float g = dropGain.getNextValue();
            dropIn[(size_t) i] = dropDry[(size_t) i] * (1.0f - g) + dropIn[(size_t) i] * g;
        }
    }

    //==============================================================================
    // Tube-screamer-style boost: tighten lows, soft asymmetric clip (mid focus),
    // tone tilt, output level. Runs on the mono DI in front of the amp.
    struct Screamer
    {
        double hpZ = 0.0, toneZ = 0.0;
        void reset() { hpZ = 0.0; toneZ = 0.0; }

        void process (double* x, int n, double sr, float drive01, float tone01, double level)
        {
            const double hpA   = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 180.0 / sr);
            const double g      = 1.0 + (double) juce::jlimit (0.0f, 1.0f, drive01) * 24.0;
            const double toneHz = 2200.0 + (double) juce::jlimit (0.0f, 1.0f, tone01) * 5500.0;
            const double lpA    = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * toneHz / sr);
            const double bias   = 0.18;                // asymmetry -> even harmonics
            const double norm   = std::tanh (bias);
            // Compensate so low drive stays near unity level.
            const double makeup = 1.0 / std::tanh (g * 0.5);

            for (int i = 0; i < n; ++i)
            {
                const double in = x[i];
                hpZ += hpA * (in - hpZ);
                const double hp = in - hpZ;            // tightened (high-passed)
                double d = std::tanh (g * hp + bias) - norm;
                d *= makeup;
                toneZ += lpA * (d - toneZ);            // tone low-pass
                x[i] = toneZ * level;
            }
        }
    };

    void loadRigs()
    {
        struct RigAsset { const char* data; int size; };
        const RigAsset assets[3] = {
            { BinaryData::rig_bite_nam, BinaryData::rig_bite_namSize },
            { BinaryData::rig_body_nam, BinaryData::rig_body_namSize },
            { BinaryData::rig_edge_nam, BinaryData::rig_edge_namSize },
        };

        for (size_t i = 0; i < 3; ++i)
        {
            try
            {
                const std::string text (assets[i].data, (size_t) assets[i].size);
                auto json = nlohmann::json::parse (text);
                rigs[i] = nam::get_dsp (json);

                loudness[i]  = (rigs[i] && rigs[i]->HasLoudness()) ? rigs[i]->GetLoudness() : kTargetLufs;
                matchGain[i] = std::pow (10.0, (kTargetLufs - loudness[i]) / 20.0);
            }
            catch (const std::exception&)
            {
                rigs[i].reset();
                loudness[i]  = kTargetLufs;
                matchGain[i] = 1.0;
            }
        }
    }

    void loadConvolvers (const juce::dsp::ProcessSpec& spec)
    {
        struct IrAsset { const char* data; int size; };
        const IrAsset irs[3] = {
            { BinaryData::ir_ashen_wav,     BinaryData::ir_ashen_wavSize },
            { BinaryData::ir_meshuggah_wav, BinaryData::ir_meshuggah_wavSize },
            { BinaryData::ir_pdi09_wav,     BinaryData::ir_pdi09_wavSize },
        };

        juce::WavAudioFormat wav;

        // Reference amp signal for level measurement: noise through the first
        // available rig, so the test source has a realistic (HF-rich) amp
        // spectrum. The cab's job is to roll that off, so this captures the
        // true cab-induced level drop.
        std::vector<double> ampRef = makeAmpReference();
        const double ampRefRms = rms (ampRef);
        ampRefStored = ampRef;            // kept for user-IR makeup measurement
        ampRefRmsStored = ampRefRms;

        for (size_t i = 0; i < 3; ++i)
        {
            convolvers[i].prepare (spec);

            // Read the IR samples (mono-summed) to measure the real level drop
            // it causes on the amp signal.
            std::vector<float> irMono;
            {
                std::unique_ptr<juce::AudioFormatReader> reader (
                    wav.createReaderFor (
                        new juce::MemoryInputStream (irs[i].data, (size_t) irs[i].size, false),
                        true));

                if (reader != nullptr && reader->lengthInSamples > 0)
                {
                    const int chs = (int) reader->numChannels;
                    const int len = (int) reader->lengthInSamples;
                    juce::AudioBuffer<float> irBuf (juce::jmax (1, chs), len);
                    reader->read (&irBuf, 0, len, 0, true, true);

                    irMono.assign ((size_t) len, 0.0f);
                    for (int ch = 0; ch < irBuf.getNumChannels(); ++ch)
                    {
                        const float* d = irBuf.getReadPointer (ch);
                        for (int k = 0; k < len; ++k)
                            irMono[(size_t) k] += d[k];
                    }
                    const float inv = 1.0f / (float) juce::jmax (1, irBuf.getNumChannels());
                    for (auto& v : irMono) v *= inv;
                }
            }

            // makeup = dryAmpRMS / cabbedAmpRMS  -> cab on/off level-matched.
            double makeup = 1.0;
            if (! irMono.empty() && ampRefRms > 1.0e-9)
            {
                const double wetRms = rmsOfConvolution (ampRef, irMono);
                if (wetRms > 1.0e-9)
                    makeup = ampRefRms / wetRms;
            }
            cabMakeup[i] = (float) juce::jlimit (0.0625, 64.0, makeup);

            // Load WITHOUT JUCE normalisation; our measured makeup handles level.
            convolvers[i].loadImpulseResponse (
                irs[i].data, (size_t) irs[i].size,
                juce::dsp::Convolution::Stereo::no,
                juce::dsp::Convolution::Trim::yes,
                0,
                juce::dsp::Convolution::Normalise::no);
        }

        // Restore rig states (the reference run disturbed the first rig).
        for (auto& rig : rigs)
            if (rig)
                rig->Reset (namRate, namMaxBlock);
    }

    // ---- level-measurement helpers -----------------------------------------
    std::vector<double> makeAmpReference()
    {
        const int L = 16384;
        std::vector<double> x ((size_t) L);
        std::uint32_t s = 22695477u;
        auto rnd = [&] { s = s * 1664525u + 1013904223u;
                         return ((double) s / (double) 0xFFFFFFFFu) * 2.0 - 1.0; };
        for (int i = 0; i < L; ++i)
            x[(size_t) i] = 0.1 * rnd();

        // Find a loaded rig to colour the noise like a real amp.
        for (size_t r = 0; r < rigs.size(); ++r)
        {
            if (! rigs[r]) continue;
            rigs[r]->Reset (sampleRate, L);
            std::vector<double> y ((size_t) L);
            double* ip[1] = { x.data() };
            double* op[1] = { y.data() };
            rigs[r]->process (ip, op, L);
            const double g = matchGain[r];
            for (int i = 0; i < L; ++i) y[(size_t) i] *= g;
            return y;
        }
        return x; // no rig: fall back to the raw noise
    }

    static double rms (const std::vector<double>& v)
    {
        if (v.empty()) return 0.0;
        double e = 0.0; for (double x : v) e += x * x;
        return std::sqrt (e / (double) v.size());
    }

    // RMS of (signal * ir), measured only over the fully-overlapped region.
    static double rmsOfConvolution (const std::vector<double>& sig, const std::vector<float>& ir)
    {
        const int H = juce::jmin ((int) ir.size(), 4096); // cab energy is up front
        const int L = (int) sig.size();
        if (H <= 0 || L <= H) return 0.0;

        double e = 0.0; long cnt = 0;
        for (int i = H; i < L; ++i)
        {
            double acc = 0.0;
            for (int k = 0; k < H; ++k)
                acc += sig[(size_t) (i - k)] * (double) ir[(size_t) k];
            e += acc * acc; ++cnt;
        }
        return cnt > 0 ? std::sqrt (e / (double) cnt) : 0.0;
    }

    //==============================================================================
    static constexpr double kTargetLufs = -18.0;

    double sampleRate = 48000.0;
    int    maxBlock   = 512;
    bool   prepared   = false;

    std::array<std::unique_ptr<nam::DSP>, 3> rigs;
    std::array<double, 3> loudness  { kTargetLufs, kTargetLufs, kTargetLufs };
    std::array<double, 3> matchGain { 1.0, 1.0, 1.0 };

    std::array<juce::dsp::Convolution, 3> convolvers;
    std::array<float, 3> cabMakeup { 1.0f, 1.0f, 1.0f }; // level-match cab on/off

    // User-loaded cab IR + NAM rig slots.
    juce::dsp::Convolution userConvolver;
    std::atomic<bool>  userIrLoaded { false };
    std::atomic<float> userCabMakeup { 1.0f };
    juce::File userIrFile;
    std::unique_ptr<nam::DSP> userRig;
    std::atomic<bool> userRigReady { false };
    double userRigMatch = 1.0;
    juce::SpinLock userRigLock;
    juce::File userRigFile;
    juce::dsp::ProcessSpec lastSpec {};
    std::vector<double> ampRefStored;
    double ampRefRmsStored = 0.0;

    juce::dsp::StateVariableTPTFilter<float> lowCut;     // output sub-bass high-pass
    juce::dsp::StateVariableTPTFilter<float> highCut;    // post-cab fizz low-pass
    bool highCutActive = false;

    // Presence: high-shelf EQ post-cab for brightness control.
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> presenceFilter;
    float lastPresenceDb = 0.0f;

    void updatePresenceCoeffs (float db)
    {
        if (std::abs (db - lastPresenceDb) < 0.01f)
            return;
        lastPresenceDb = db;
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 3500.0f, 0.707f,
            juce::Decibels::decibelsToGain (db));
        *presenceFilter.state = *coeffs;
    }

    std::vector<std::vector<float>> dryScratch;          // pre-cab signal for blend
    juce::SmoothedValue<float> cabMixSmoothed;           // click-free cab dry/wet

    std::vector<double> monoIn, rigOut, mixBuf;

    // 48 kHz resampling state (only active when host rate != 48 kHz)
    bool   resampling  = false;
    double namRate     = 48000.0;
    int    namMaxBlock = 512;
    int    latencySamples = 0;
    RateConverter srcUp, srcDown;
    std::vector<float>  monoInF, up48f, nam48fOut, hostOutFifo;
    std::vector<double> nam48, nam48out;

    float gateGain = 1.0f;
    bool  gateOpen = true;
    int   gateHoldCounter = 0;

    double tightLp1 = 0.0, tightLp2 = 0.0; // pre-amp high-pass state
    Screamer screamer;

    // Drop pedal
    apex::dsp::PitchShifter dropMain, dropSubShifter;
    std::vector<float> dropIn, dropDry, dropSubBuf;
    juce::SmoothedValue<float> dropGain, subLevel;
    bool dropWasActive = false;
    Biquad subLowpass;

    // Amp EQ: bass shelf, mid peak, treble shelf
    Biquad ampEq[4];
    float lastEq[4] { 1.0e9f, 1.0e9f, 1.0e9f, 1.0e9f };

    // Shape pedal, Void unit
    apex::dsp::ChugShaper chugShaper;
    apex::dsp::LowDirt lowDirt;
    std::vector<float> shapeDi, shapeBuf;
    apex::dsp::StereoDelay echo;
    apex::dsp::AbyssReverb abyss;
};
