#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "apex/ui/Materials.h"

#include <functional>
#include <memory>

namespace apex::ui
{

//==============================================================================
/** A hardware knob. Drag up/down (Shift for fine), wheel, double-click resets,
    arrow keys when focused; the value shows in a bubble while hovering or
    dragging. Drawn by ApexLookAndFeel. */
class Knob : public juce::Slider
{
public:
    explicit Knob (KnobStyle style = KnobStyle::spunAluminium);

    KnobStyle getStyle() const noexcept { return style; }

    /** Where the value bubble is added (the editor's scaled content). */
    void setPopupParent (juce::Component* parent);

    /** The component is 1.5 x the knob so its shadow is not clipped; the knob
        itself is the centred circle (also the mouse hit area). */
    static constexpr float boundsRatio = 1.5f;
    juce::Rectangle<float> getKnobArea() const;
    bool hitTest (int x, int y) override;

private:
    KnobStyle style;
};

/** Binds a knob to a parameter; the bubble shows "value unit", double-click
    returns to the parameter default and each drag is one undo step. */
std::unique_ptr<juce::SliderParameterAttachment> attach (Knob&, juce::RangedAudioParameter&, juce::UndoManager*);

//==============================================================================
/** Chrome stomp switch (latching). */
class Footswitch : public juce::Button
{
public:
    Footswitch();
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

/** Chrome bat-handle toggle (up = on). */
class BatToggle : public juce::Button
{
public:
    BatToggle();
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

/** Small square hardware push button with a chevron. */
class HardwareButton : public juce::Button
{
public:
    enum class Glyph { up, down, left, right, load };
    explicit HardwareButton (Glyph);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Glyph glyph;
};

//==============================================================================
/** LED with chrome bezel; the component is larger than the bezel to leave room
    for the glow (bezel = 30 % of the width). */
class Led : public juce::Component
{
public:
    explicit Led (juce::Colour colour = juce::Colour (0xffff3b1f));
    void setOn (bool shouldBeOn);
    bool isOn() const noexcept { return on; }
    void paint (juce::Graphics&) override;

private:
    juce::Colour colour;
    bool on = false;
};

/** Faceted pilot jewel (bezel = 28 % of the width). */
class Jewel : public juce::Component
{
public:
    void setOn (bool shouldBeOn);
    void paint (juce::Graphics&) override;

private:
    bool on = true;
};

//==============================================================================
/** Rotary selector with a chicken-head knob and silkscreened positions. Click
    a label, click/drag the knob, scroll, or use the arrow keys. */
class RotarySelector : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    RotarySelector (juce::StringArray labels, float firstAngleDegrees, float stepDegrees);

    void setSelected (int index, juce::NotificationType);
    int getSelected() const noexcept { return selected; }
    void setItemEnabled (int index, bool enabled);

    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    juce::Point<float> knobCentre() const;
    float knobDiameter() const;
    float angleFor (int index) const;
    juce::Point<float> labelCentre (int index) const;
    void step (int delta);

    juce::StringArray labels;
    juce::Array<bool> enabled;
    float firstAngle, stepAngle;
    int selected = 0;
    float dragAccumulator = 0.0f;
    juce::Point<float> lastDrag;
};

//==============================================================================
/** Seven-segment LED readout (draw the glass behind it). */
class SevenSegmentDisplay : public juce::Component
{
public:
    explicit SevenSegmentDisplay (int numCells = 3);
    void setText (const juce::String&);
    void setLitColour (juce::Colour);
    void paint (juce::Graphics&) override;

private:
    juce::String text;
    juce::Colour lit;
    int cells;
};

/** Amber dot-matrix style text (draw the glass behind it). */
class LcdText : public juce::Component
{
public:
    void setText (const juce::String& primary, const juce::String& secondary = {});
    void paint (juce::Graphics&) override;

private:
    juce::String primary, secondary;
};

//==============================================================================
/** Horizontal LED-ladder level meter with peak hold and clip light. */
class LevelMeter : public juce::Component
{
public:
    /** Feed the block peak (linear); ballistics are applied here (call ~30 Hz). */
    void pushPeak (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f, peakHold = 0.0f;
    int holdFrames = 0, clipFrames = 0;
};

} // namespace apex::ui
