#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Presets.h"
#include "ApexLookAndFeel.h"
#include <functional>
#include <vector>
#include <cmath>

/** Small tuner readout: note name + cents needle, polled from the processor. */
struct TunerComponent : public juce::Component, private juce::Timer
{
    std::function<float()> getFreq;

    TunerComponent() { startTimerHz (20); }
    ~TunerComponent() override { stopTimer(); }

    void timerCallback() override
    {
        if (getFreq) { freq = getFreq(); repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff15171b));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

        auto r = getLocalBounds().reduced (6);

        if (freq <= 0.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.setFont (juce::Font (13.0f, juce::Font::bold));
            g.drawText ("TUNER  --", r, juce::Justification::centred);
            return;
        }

        const double midi = 69.0 + 12.0 * std::log2 (freq / 440.0);
        const int nearest = (int) std::lround (midi);
        const double cents = (midi - (double) nearest) * 100.0;
        static const char* names[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        const int idx = ((nearest % 12) + 12) % 12;
        const bool inTune = std::abs (cents) < 5.0;

        g.setColour (inTune ? juce::Colour (0xff39d353) : juce::Colours::white.withAlpha (0.9f));
        g.setFont (juce::Font (18.0f, juce::Font::bold));
        g.drawText (juce::String (names[idx]), r.removeFromLeft (38), juce::Justification::centred);

        auto bar = r.reduced (4, 9).toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRect (bar);
        const float cx = bar.getCentreX();
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawVerticalLine ((int) cx, bar.getY(), bar.getBottom());

        float pos = cx + (float) (cents / 50.0) * (bar.getWidth() * 0.5f);
        pos = juce::jlimit (bar.getX(), bar.getRight(), pos);
        g.setColour (inTune ? juce::Colour (0xff39d353) : juce::Colour (0xffff7a18));
        g.fillRect (juce::Rectangle<float> (pos - 2.0f, bar.getY(), 4.0f, bar.getHeight()));
    }

    float freq = 0.0f;
};

/** IN/OUT level meters with decay ballistics + peak-hold, dB-scaled. */
struct MetersComponent : public juce::Component, private juce::Timer
{
    std::function<float()> getIn, getOut;

    MetersComponent() { startTimerHz (30); }
    ~MetersComponent() override { stopTimer(); }

    void timerCallback() override
    {
        auto upd = [] (float lvl, float& sm, float& pk)
        {
            sm = juce::jmax (lvl, sm * 0.80f);          // fast attack, smooth decay
            pk = (lvl > pk) ? lvl : pk * 0.94f;          // peak hold
        };
        if (getIn)  upd (getIn(),  inSm,  inPk);
        if (getOut) upd (getOut(), outSm, outPk);
        repaint();
    }

    static float toNorm (float lin)
    {
        const float db = juce::Decibels::gainToDecibels (lin, -60.0f);
        return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    }

    void drawBar (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& label,
                  float sm, float pk)
    {
        auto lab = r.removeFromLeft (30);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText (label, lab, juce::Justification::centredLeft);

        auto bar = r.toFloat().reduced (0.0f, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (bar, 2.0f);

        const float n = toNorm (sm);
        if (n > 0.001f)
        {
            auto fill = bar.withWidth (bar.getWidth() * n);
            const juce::Colour col = n > 0.92f ? juce::Colour (0xffe23b3b)
                                   : n > 0.78f ? juce::Colour (0xffffa54d)
                                               : juce::Colour (0xff39d353);
            g.setColour (col);
            g.fillRoundedRectangle (fill, 2.0f);
        }
        const float pn = toNorm (pk);
        if (pn > 0.001f)
        {
            const float px = bar.getX() + bar.getWidth() * pn;
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRect (px - 1.0f, bar.getY(), 1.6f, bar.getHeight());
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        drawBar (g, r.removeFromTop (r.getHeight() / 2), "IN",  inSm,  inPk);
        drawBar (g, r,                                   "OUT", outSm, outPk);
    }

    float inSm = 0, outSm = 0, inPk = 0, outPk = 0;
};

/** A labelled rotary knob bundling a Slider + attachment + caption. */
struct Knob : public juce::Component
{
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramID,
          const juce::String& caption)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
        addAndMakeVisible (slider);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (13.0f, juce::Font::bold));
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            s, paramID, slider);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
    }

    juce::Slider slider;
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ApexAmpEditor : public juce::AudioProcessorEditor
{
public:
    explicit ApexAmpEditor (ApexAmpProcessor&);
    ~ApexAmpEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ApexAmpProcessor& proc;
    ApexLookAndFeel lnf;

    juce::ComboBox presetBox;
    juce::TextButton savePresetButton { "Save" }, loadPresetButton { "Load" };
    TunerComponent tuner;
    MetersComponent meters;

    juce::ComboBox channelBox, tonestackBox, cabTypeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelAtt, tonestackAtt, cabTypeAtt;

    juce::ToggleButton cabButton { "Cab" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cabAtt;
    juce::TextButton loadIRButton { "IR A..." };
    juce::TextButton loadIRBButton { "IR B..." };
    juce::TextButton clearIRButton { "Built-in" };
    juce::Label      irLabel;

    // Knobs
    std::unique_ptr<Knob> kInput, kGain, kPush, kTight, kSuperCut, kBias,
                          kBass, kMid, kTreble, kChug, kLowDrv, kLowMix, kBlend,
                          kSag, kPower, kGate, kPunch, kLoud, kMaster;

    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> rcHeader, rcPreamp, rcTone, rcDyn, rcCab, rcOut;
    void layoutRects();
    void updateIRLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApexAmpEditor)
};
