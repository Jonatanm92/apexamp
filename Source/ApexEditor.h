#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// Dark, modern amp-sim look: arc-fill rotary knobs, styled toggles and combos.
//==============================================================================
class ApexLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ApexLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isDown,
                       int bx, int by, int bw, int bh, juce::ComboBox&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;

    static const juce::Colour accent;
    static const juce::Colour accent2;
    static const juce::Colour panelTop;
    static const juce::Colour panelBot;
    static const juce::Colour border;
    static const juce::Colour text;
    static const juce::Colour textDim;
};

//==============================================================================
class ApexAmpEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit ApexAmpEditor (ApexAmpProcessor&);
    ~ApexAmpEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    using APVTS = juce::AudioProcessorValueTreeState;
    using SA = APVTS::SliderAttachment;
    using CA = APVTS::ComboBoxAttachment;
    using BA = APVTS::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  nameLabel;
        std::unique_ptr<SA> att;
    };

    Knob& addKnob (const juce::String& paramID, const juce::String& name,
                   const juce::String& suffix, int decimals,
                   const juce::String& tooltip = {});

    void applyPreset (int index);

    void drawPanel  (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);
    void drawMeter  (juce::Graphics&, juce::Rectangle<int>, float level01, const juce::String& label);
    void drawHeader (juce::Graphics&, juce::Rectangle<int>);

    ApexAmpProcessor& proc;
    ApexLookAndFeel lnf;
    juce::TooltipWindow tooltipWindow { this, 600 };

    juce::OwnedArray<Knob> knobs;
    Knob* inputGain  = nullptr;
    Knob* tight      = nullptr;
    Knob* gateThresh = nullptr;
    Knob* gateHold   = nullptr;
    Knob* mixBite    = nullptr;
    Knob* mixBody    = nullptr;
    Knob* mixEdge    = nullptr;
    Knob* cabMix     = nullptr;
    Knob* presence   = nullptr;
    Knob* lowCut     = nullptr;
    Knob* outputGain = nullptr;

    juce::ComboBox rigModeBox, rigBox, irBox;
    std::unique_ptr<CA> rigModeAtt, rigAtt, irAtt;

    juce::Label rigModeLabel, rigLabel, irLabel;

    juce::ToggleButton gateButton { "GATE" };
    std::unique_ptr<BA> gateAtt;

    // preset bar
    juce::ComboBox  presetBox;
    juce::TextButton prevPresetButton { "<" }, nextPresetButton { ">" };
    juce::TextButton savePresetButton { "SAVE" }, loadPresetButton { "LOAD" };
    juce::TextButton aboutButton { "?" };
    juce::ToggleButton bypassButton { "BYPASS" };
    std::unique_ptr<BA> bypassAtt;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // cached panel rects (set in resized(), used in paint())
    juce::Rectangle<int> headerArea, inputPanel, rigPanel, cabPanel, outPanel;

    // smoothed meter values
    float inMeter = 0.0f, outMeter = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApexAmpEditor)
};
