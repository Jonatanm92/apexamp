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
