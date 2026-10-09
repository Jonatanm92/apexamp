#pragma once

#include "DropProcessor.h"
#include "apex/ui/Controls.h"
#include "apex/ui/EditorBase.h"

/**
 * DropEditor
 * ----------
 * Apex Drop as a single hardware unit: a big seven-segment shift display,
 * Live / Studio toggle, Fine, Body, Sub, Mix and Output, and a stomp switch
 * for bypass.
 */
class DropEditor : public apex::ui::EditorBase
{
public:
    explicit DropEditor (DropProcessor&);
    ~DropEditor() override;

protected:
    void tick() override;
    float getInputPeak() override  { return proc.inputMagnitude.load(); }
    float getOutputPeak() override { return proc.outputMagnitude.load(); }

private:
    struct DropStage;

    apex::ui::Knob& makeKnob (const juce::String& paramId, juce::Point<float> centre, float diameter, const juce::String& tip);
    void stepShift (int delta);

    DropProcessor& proc;
    std::unique_ptr<DropStage> dropStage;

    juce::OwnedArray<apex::ui::Knob> knobs;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> knobAttachments;

    apex::ui::SevenSegmentDisplay digits { 3 };
    apex::ui::LcdText caption;
    apex::ui::HardwareButton up { apex::ui::HardwareButton::Glyph::up }, down { apex::ui::HardwareButton::Glyph::down };
    apex::ui::BatToggle modeToggle;
    apex::ui::Footswitch stomp;
    apex::ui::Led led { juce::Colour (0xffff3b1f) };

    std::unique_ptr<juce::ParameterAttachment> shiftAttachment, modeAttachment, bypassAttachment;
};
