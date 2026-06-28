#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Presets.h"

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
    ~ApexAmpEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ApexAmpProcessor& proc;

    juce::ComboBox presetBox;
    juce::TextButton savePresetButton { "Save" }, loadPresetButton { "Load" };

    juce::ComboBox channelBox, tonestackBox, cabTypeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelAtt, tonestackAtt, cabTypeAtt;

    juce::ToggleButton cabButton { "Cab" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cabAtt;
    juce::TextButton loadIRButton { "Load IR..." };
    juce::TextButton clearIRButton { "Built-in" };
    juce::Label      irLabel;

    // Knobs
    std::unique_ptr<Knob> kInput, kGain, kPush, kTight, kSuperCut, kBias,
                          kBass, kMid, kTreble, kChug, kLowDrv, kLowMix,
                          kSag, kPower, kGate, kPunch, kLoud, kMaster;

    std::unique_ptr<juce::FileChooser> chooser;

    void updateIRLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApexAmpEditor)
};
