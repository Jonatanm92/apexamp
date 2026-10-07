#pragma once

#include "PluginProcessor.h"
#include "apex/ui/Controls.h"
#include "apex/ui/EditorBase.h"

/**
 * AmpEditor
 * ---------
 * The ApexAmp rig: a NAM-powered head on a 4x12, and a pedalboard in front of
 * it (Drop, Gate, Boost, Cab). Everything static is painted once into a cached
 * image (AmpStage); the controls on top are live components bound to the
 * parameters with undo.
 */
class AmpEditor : public apex::ui::EditorBase
{
public:
    explicit AmpEditor (ApexAmpProcessor&);
    ~AmpEditor() override;

    /** Design-space layout shared by the static painter and resized(). */
    struct Layout
    {
        Layout();
        juce::Rectangle<float> head, plate, cab, board;
        juce::Rectangle<float> drop, gate, boost, cabUnit;            // pedal outer bounds
        juce::Rectangle<float> dropTop, gateTop, boostTop, cabTop;    // top faces
        juce::Point<float> gain, tight, bass, mid, treble, presence, master;
        float bigKnob = 84.0f, knob = 58.0f;
        juce::Rectangle<float> selector, trims, power, jewel;
        juce::Rectangle<float> dropGlass, dropDigits, dropCaption, dropUp, dropDown;
        juce::Point<float> dropBody, dropSub, gateThresh, gateHold, boostDrive, boostTone, boostLevel, cabMix, cabLowCut;
        juce::Rectangle<float> cabGlass, cabLcd, cabPrev, cabNext, cabLoad;
    };

protected:
    void resized() override;
    void tick() override;
    float getInputPeak() override;
    float getOutputPeak() override;
    void addSettingsItems (juce::PopupMenu&) override;
    void handleSettingsItem (int) override;

private:
    struct AmpStage;
    struct BlendTrims;

    apex::ui::Knob& makeKnob (const juce::String& paramId, apex::ui::KnobStyle, juce::Point<float> centre, float diameter);
    apex::ui::Footswitch& makeFootswitch (const juce::String& paramId, apex::ui::Led& led, juce::Rectangle<float> top);
    void syncSelector();
    void selectRig (int position);
    void stepParam (juce::ParameterAttachment&, juce::RangedAudioParameter&, int delta, int minValue, int maxValue);
    void loadRig();
    void loadIr();

    ApexAmpProcessor& proc;
    Layout layout;
    std::unique_ptr<AmpStage> ampStage;

    juce::OwnedArray<apex::ui::Knob> knobs;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> knobAttachments;
    juce::OwnedArray<apex::ui::Footswitch> footswitches;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttonAttachments;

    apex::ui::RotarySelector selector;
    std::unique_ptr<BlendTrims> trims;
    apex::ui::BatToggle power;
    apex::ui::Jewel jewel;
    apex::ui::Led dropLed, gateLed, boostLed;
    apex::ui::SevenSegmentDisplay dropDigits { 3 };
    apex::ui::LcdText dropCaption, cabLcd;
    apex::ui::HardwareButton dropUp { apex::ui::HardwareButton::Glyph::up }, dropDown { apex::ui::HardwareButton::Glyph::down };
    apex::ui::HardwareButton cabPrev { apex::ui::HardwareButton::Glyph::left }, cabNext { apex::ui::HardwareButton::Glyph::right };
    apex::ui::HardwareButton cabLoad { apex::ui::HardwareButton::Glyph::load };

    std::unique_ptr<juce::ParameterAttachment> rigModeAttachment, rigAttachment, bypassAttachment,
                                               dropShiftAttachment, irAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
};
