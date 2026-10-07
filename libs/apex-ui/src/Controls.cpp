#include "apex/ui/Controls.h"
#include "apex/ui/Theme.h"

#include <cmath>

namespace apex::ui
{

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
}

//==============================================================================
Knob::Knob (KnobStyle s) : style (s)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (1.25f * pi, 2.75f * pi, true);
    setMouseDragSensitivity (220);
    // Shift switches to velocity mode with a low sensitivity: fine adjustment
    // without the value jumping when Shift is pressed mid-drag.
    setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void Knob::setPopupParent (juce::Component* parent)
{
    setPopupDisplayEnabled (true, true, parent, 1200);
}

juce::Rectangle<float> Knob::getKnobArea() const
{
    const auto b = getLocalBounds().toFloat();
    const float d = juce::jmin (b.getWidth(), b.getHeight()) / boundsRatio;
    return b.withSizeKeepingCentre (d, d);
}

bool Knob::hitTest (int x, int y)
{
    const auto area = getKnobArea();
    return area.getCentre().getDistanceFrom ({ (float) x, (float) y }) <= area.getWidth() * 0.6f;
}

std::unique_ptr<juce::SliderParameterAttachment> attach (Knob& knob, juce::RangedAudioParameter& param, juce::UndoManager* undo)
{
    auto attachment = std::make_unique<juce::SliderParameterAttachment> (param, knob, undo);
    const auto label = param.getLabel();
    knob.textFromValueFunction = [&param, label] (double value)
    {
        auto text = param.getText (param.convertTo0to1 ((float) value), 0).trim();
        if (label.isNotEmpty() && ! text.endsWithIgnoreCase (label))
            text << " " << label;
        return text.replace ("-", juce::String::charToString (0x2212));
    };
    knob.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
    knob.setTitle (param.getName (64));
    return attachment;
}

//==============================================================================
Footswitch::Footswitch() : juce::Button ("Footswitch")
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Footswitch::paintButton (juce::Graphics& g, bool, bool down)
{
    draw::footswitch (g, getLocalBounds().toFloat().reduced (getWidth() * 0.12f), down);
}

BatToggle::BatToggle() : juce::Button ("Toggle")
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void BatToggle::paintButton (juce::Graphics& g, bool, bool)
{
    draw::batToggle (g, getLocalBounds().toFloat(), getToggleState());
}

HardwareButton::HardwareButton (Glyph gl) : juce::Button ("Button"), glyph (gl)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void HardwareButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (1.5f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.22f;

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (b.translated (0.0f, 1.5f), r);
    g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xff1a1a1d : 0xff35353a), b.getX(), b.getY(),
                                             juce::Colour (down ? 0xff0e0e10 : 0xff141416), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, r);
    g.setColour (juce::Colours::white.withAlpha (down ? 0.04f : 0.12f));
    g.drawRoundedRectangle (b.reduced (0.5f), r, 1.0f);

    const auto c = b.getCentre().translated (0.0f, down ? 0.8f : 0.0f);
    const float s = juce::jmin (b.getWidth(), b.getHeight()) * 0.2f;
    juce::Path p;
    switch (glyph)
    {
        case Glyph::up:    p.startNewSubPath (c.x - s, c.y + s * 0.5f); p.lineTo (c.x, c.y - s * 0.5f); p.lineTo (c.x + s, c.y + s * 0.5f); break;
        case Glyph::down:  p.startNewSubPath (c.x - s, c.y - s * 0.5f); p.lineTo (c.x, c.y + s * 0.5f); p.lineTo (c.x + s, c.y - s * 0.5f); break;
        case Glyph::left:  p.startNewSubPath (c.x + s * 0.5f, c.y - s); p.lineTo (c.x - s * 0.5f, c.y); p.lineTo (c.x + s * 0.5f, c.y + s); break;
        case Glyph::right: p.startNewSubPath (c.x - s * 0.5f, c.y - s); p.lineTo (c.x + s * 0.5f, c.y); p.lineTo (c.x - s * 0.5f, c.y + s); break;
        case Glyph::load:
            p.startNewSubPath (c.x - s, c.y + s * 0.2f); p.lineTo (c.x - s, c.y + s); p.lineTo (c.x + s, c.y + s); p.lineTo (c.x + s, c.y + s * 0.2f);
            p.startNewSubPath (c.x, c.y - s); p.lineTo (c.x, c.y + s * 0.4f);
            p.startNewSubPath (c.x - s * 0.5f, c.y - s * 0.1f); p.lineTo (c.x, c.y + s * 0.4f); p.lineTo (c.x + s * 0.5f, c.y - s * 0.1f);
            break;
    }
    g.setColour (over ? colours::bone : colours::label);
    g.strokePath (p, juce::PathStrokeType (juce::jmax (1.4f, s * 0.32f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
Led::Led (juce::Colour c) : colour (c)
{
    setInterceptsMouseClicks (false, false);
}

void Led::setOn (bool shouldBeOn)
{
    if (on != shouldBeOn)
    {
        on = shouldBeOn;
        repaint();
    }
}

void Led::paint (juce::Graphics& g)
{
    const float d = (float) juce::jmin (getWidth(), getHeight()) * 0.3f;
    draw::led (g, getLocalBounds().toFloat().withSizeKeepingCentre (d, d), on, colour);
}

void Jewel::setOn (bool shouldBeOn)
{
    if (on != shouldBeOn)
    {
        on = shouldBeOn;
        repaint();
    }
}

void Jewel::paint (juce::Graphics& g)
{
    const float d = (float) juce::jmin (getWidth(), getHeight()) * 0.28f;
    draw::jewel (g, getLocalBounds().toFloat().withSizeKeepingCentre (d, d), on, colours::amber);
}

//==============================================================================
RotarySelector::RotarySelector (juce::StringArray l, float firstDeg, float stepDeg)
    : labels (std::move (l)), firstAngle (juce::degreesToRadians (firstDeg)), stepAngle (juce::degreesToRadians (stepDeg))
{
    for (int i = 0; i < labels.size(); ++i)
        enabled.add (true);
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Rig");
}

void RotarySelector::setSelected (int index, juce::NotificationType notification)
{
    index = juce::jlimit (0, labels.size() - 1, index);
    if (index == selected)
        return;
    selected = index;
    repaint();
    if (notification != juce::dontSendNotification && onChange)
        onChange (selected);
}

void RotarySelector::setItemEnabled (int index, bool shouldBeEnabled)
{
    if (juce::isPositiveAndBelow (index, enabled.size()))
    {
        enabled.set (index, shouldBeEnabled);
        repaint();
    }
}

float RotarySelector::knobDiameter() const    { return (float) getWidth() * 0.44f; }
juce::Point<float> RotarySelector::knobCentre() const { return { (float) getWidth() * 0.5f, (float) getHeight() * 0.62f }; }
float RotarySelector::angleFor (int index) const       { return firstAngle + stepAngle * (float) index; }

juce::Point<float> RotarySelector::labelCentre (int index) const
{
    const auto c = knobCentre();
    const float r = knobDiameter() * 0.9f;
    const float a = angleFor (index);
    return { c.x + std::sin (a) * r, c.y - std::cos (a) * r };
}

void RotarySelector::paint (juce::Graphics& g)
{
    const auto c = knobCentre();
    const float d = knobDiameter();

    for (int i = 0; i < labels.size(); ++i)
    {
        const float a = angleFor (i);
        const bool sel = i == selected;
        const auto dot = juce::Point<float> (c.x + std::sin (a) * d * 0.64f, c.y - std::cos (a) * d * 0.64f);
        g.setColour (sel ? colours::amber : colours::faint);
        g.fillEllipse (juce::Rectangle<float> (d * 0.06f, d * 0.06f).withCentre (dot));
        const auto colour = ! enabled[i] ? colours::faint.withAlpha (0.6f) : (sel ? colours::bone : colours::dim);
        draw::silkscreen (g, labels[i], Fonts::label (d * 0.17f, 0.14f),
                          juce::Rectangle<float> (d * 0.9f, d * 0.22f).withCentre (labelCentre (i)),
                          juce::Justification::centred, colour);
    }

    draw::knob (g, juce::Rectangle<float> (d, d).withCentre (c), angleFor (selected), KnobStyle::chickenHead);

    if (hasKeyboardFocus (false))
    {
        g.setColour (colours::amber.withAlpha (0.4f));
        g.drawEllipse (juce::Rectangle<float> (d * 1.2f, d * 1.2f).withCentre (c), 1.0f);
    }
}

void RotarySelector::step (int delta)
{
    int next = selected;
    for (int tries = 0; tries < labels.size(); ++tries)
    {
        next = juce::jlimit (0, labels.size() - 1, next + delta);
        if (enabled[next] || next == 0 || next == labels.size() - 1)
            break;
    }
    if (enabled[next])
        setSelected (next, juce::sendNotificationSync);
}

void RotarySelector::mouseDown (const juce::MouseEvent& e)
{
    dragAccumulator = 0.0f;
    lastDrag = e.position;
    for (int i = 0; i < labels.size(); ++i)
        if (e.position.getDistanceFrom (labelCentre (i)) < knobDiameter() * 0.3f)
        {
            if (enabled[i])
                setSelected (i, juce::sendNotificationSync);
            return;
        }

    if (e.position.getDistanceFrom (knobCentre()) < knobDiameter() * 0.6f && e.getNumberOfClicks() == 1)
        step (e.mods.isRightButtonDown() ? -1 : 1);
}

void RotarySelector::mouseDrag (const juce::MouseEvent& e)
{
    dragAccumulator += (e.position.x - lastDrag.x) - (e.position.y - lastDrag.y);
    lastDrag = e.position;
    if (std::abs (dragAccumulator) > 24.0f)
    {
        step (dragAccumulator > 0.0f ? 1 : -1);
        dragAccumulator = 0.0f;
    }
}

void RotarySelector::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (std::abs (wheel.deltaY) > 0.01f)
        step (wheel.deltaY > 0.0f ? 1 : -1);
}

bool RotarySelector::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::rightKey || key == juce::KeyPress::upKey)   { step (1);  return true; }
    if (key == juce::KeyPress::leftKey  || key == juce::KeyPress::downKey) { step (-1); return true; }
    return false;
}

//==============================================================================
SevenSegmentDisplay::SevenSegmentDisplay (int numCells) : lit (colours::amber), cells (numCells)
{
    setInterceptsMouseClicks (false, false);
}

void SevenSegmentDisplay::setText (const juce::String& t)
{
    if (text != t)
    {
        text = t;
        repaint();
    }
}

void SevenSegmentDisplay::setLitColour (juce::Colour c)
{
    lit = c;
    repaint();
}

void SevenSegmentDisplay::paint (juce::Graphics& g)
{
    draw::sevenSegment (g, text, getLocalBounds().toFloat(), lit, cells);
}

void LcdText::setText (const juce::String& p, const juce::String& s)
{
    if (p != primary || s != secondary)
    {
        primary = p;
        secondary = s;
        repaint();
    }
}

void LcdText::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float h = b.getHeight();
    auto drawGlow = [&] (const juce::String& t, juce::Rectangle<float> area, const juce::Font& f, juce::Justification j, float alpha)
    {
        g.setFont (f);
        g.setColour (colours::amber.withAlpha (0.25f * alpha));
        for (auto o : { juce::Point<float> (-0.8f, 0.0f), { 0.8f, 0.0f }, { 0.0f, -0.8f }, { 0.0f, 0.8f } })
            g.drawText (t, area.translated (o.x, o.y), j, true);
        g.setColour (colours::amber.withAlpha (alpha));
        g.drawText (t, area, j, true);
    };

    if (secondary.isEmpty())
    {
        drawGlow (primary, b, Fonts::monoBold (juce::jmin (h * 0.7f, 15.0f)), juce::Justification::centred, 1.0f);
        return;
    }
    drawGlow (primary, b.removeFromTop (h * 0.58f), Fonts::monoBold (h * 0.36f), juce::Justification::bottomLeft, 1.0f);
    drawGlow (secondary, b, Fonts::mono (h * 0.24f), juce::Justification::centredLeft, 0.6f);
}

//==============================================================================
void LevelMeter::pushPeak (float linearPeak)
{
    const float target = juce::jlimit (0.0f, 1.2f, linearPeak);
    level = target > level ? target : level * 0.86f;
    if (target >= peakHold)       { peakHold = target; holdFrames = 30; }
    else if (--holdFrames < 0)    peakHold *= 0.94f;
    if (target >= 0.999f)         clipFrames = 45;
    else if (clipFrames > 0)      --clipFrames;
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto clipArea = b.removeFromRight (b.getHeight() * 0.9f);
    b.removeFromRight (3.0f);

    const int n = 24;
    const float gap = 1.5f;
    const float w = (b.getWidth() - gap * (float) (n - 1)) / (float) n;
    auto toPos = [] (float lin) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (lin, -60.0f) + 60.0f) / 60.0f); };
    const int lit = juce::roundToInt (toPos (level) * (float) n);
    const int hold = juce::roundToInt (toPos (peakHold) * (float) n) - 1;

    for (int i = 0; i < n; ++i)
    {
        const auto seg = juce::Rectangle<float> (b.getX() + (float) i * (w + gap), b.getY(), w, b.getHeight());
        const bool hot = i >= n - 3;
        juce::Colour c = hot ? colours::amber : colours::bone.withAlpha (0.85f);
        if (i < lit || i == hold)
            g.setColour (i == hold && i >= lit ? c.withAlpha (0.7f) : c);
        else
            g.setColour (juce::Colour (0xff1d1d20));
        g.fillRoundedRectangle (seg, 1.0f);
    }

    g.setColour (clipFrames > 0 ? colours::danger : juce::Colour (0xff2a1512));
    g.fillRoundedRectangle (clipArea, 2.0f);
}

} // namespace apex::ui
