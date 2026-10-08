#include "apex/ui/Abyss.h"
#include "apex/ui/Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>

namespace apex::ui::abyss
{

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float startAngle = 1.25f * pi, endAngle = 2.75f * pi;

    inline float rand01 (std::uint32_t& state) noexcept
    {
        state = state * 1664525u + 1013904223u;
        return (float) (state >> 8) * (1.0f / 16777216.0f);
    }

    float physicalScale (juce::Graphics& g)
    {
        return juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    }

    /** Dark spun / knurled knob cap with a bevelled rim, lit from the top left. */
    juce::Image knobBody (int px)
    {
        px = juce::jlimit (8, 1024, px);
        auto& slot = detail::cachedImage ("knob:" + juce::String (px));
        if (slot.isValid())
            return slot;

        juce::Image img (juce::Image::ARGB, px, px, true, juce::SoftwareImageType());
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const float R = (float) px * 0.5f;
        const float light = -2.35f;   // top-left
        for (int y = 0; y < px; ++y)
            for (int x = 0; x < px; ++x)
            {
                const float dx = (float) x + 0.5f - R, dy = (float) y + 0.5f - R;
                const float d = std::sqrt (dx * dx + dy * dy);
                const float r = d / R;
                if (r > 1.0f + 1.0f / R)
                    continue;
                const float a = std::atan2 (dy, dx);
                const float edgeAlpha = juce::jlimit (0.0f, 1.0f, (1.0f - r) * R + 0.5f);

                float L = 0.075f;
                if (r > 0.84f)
                {
                    // knurled, bevelled skirt
                    const float knurl = 0.5f + 0.5f * std::sin (a * 48.0f);
                    const float facing = std::cos (a - light);
                    L = 0.05f + 0.06f * facing + 0.035f * knurl * (0.6f + 0.4f * facing);
                    if (r > 0.97f) L *= 0.6f;
                }
                else
                {
                    // spun cap: fine rings + a bow-tie highlight across the light axis
                    const float rings = 0.5f + 0.5f * std::sin (r * 140.0f);
                    const float bow = std::pow (std::abs (std::cos (a - light)), 10.0f);
                    L = 0.055f + 0.012f * rings + 0.10f * bow * (0.35f + 0.65f * r);
                    L += 0.03f * juce::jmax (0.0f, 1.0f - r * 1.6f);   // dome
                    if (r > 0.80f) L *= 0.55f;                          // groove before the skirt
                }
                const float v = juce::jlimit (0.0f, 1.0f, L);
                bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (v * 1.02f, v * 0.97f, v * 0.94f, edgeAlpha));
            }
        slot = img;
        return img;
    }

    juce::Rectangle<float> centredSquare (juce::Rectangle<float> b)
    {
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        return b.withSizeKeepingCentre (d, d);
    }
}

//==============================================================================
std::unique_ptr<juce::SliderParameterAttachment> attach (juce::Slider& slider, juce::RangedAudioParameter& param, juce::UndoManager* undo)
{
    auto attachment = std::make_unique<juce::SliderParameterAttachment> (param, slider, undo);
    const auto label = param.getLabel();
    slider.textFromValueFunction = [&param, label] (double value)
    {
        auto text = param.getText (param.convertTo0to1 ((float) value), 0).trim();
        if (label.isNotEmpty() && ! text.endsWithIgnoreCase (label) && text.containsAnyOf ("0123456789"))
            text << " " << label;
        return text.replace ("-", juce::String::charToString (0x2212));
    };
    slider.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
    slider.setTitle (param.getName (64));
    return attachment;
}

//==============================================================================
Knob::Knob()
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (startAngle, endAngle, true);
    setMouseDragSensitivity (220);
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
    auto b = getLocalBounds().toFloat();
    if (caption.isNotEmpty())
        b.removeFromTop (captionHeight);
    const float d = juce::jmin (b.getWidth(), b.getHeight()) / boundsRatio;
    return b.withSizeKeepingCentre (d, d);
}

bool Knob::hitTest (int x, int y)
{
    const auto area = getKnobArea();
    return area.getCentre().getDistanceFrom ({ (float) x, (float) y }) <= area.getWidth() * 0.66f;
}

void Knob::paint (juce::Graphics& g)
{
    const auto area = getKnobArea();
    const auto c = area.getCentre();
    const float d = area.getWidth();
    const bool showNumbers = numbers && d >= 44.0f;
    const int ticks = 21;
    const float tickR0 = d * 0.56f, tickR1 = d * 0.61f, majorR1 = d * 0.635f;

    // static layer: shadow, bezel, cap, dim scale
    const float phys = physicalScale (g);
    const auto key = juce::String (getWidth()) + "x" + juce::String (getHeight()) + "@" + juce::String (phys, 2) + (showNumbers ? "n" : "");
    if (cache.isNull() || key != cacheKey)
    {
        cacheKey = key;
        const int W = juce::roundToInt ((float) getWidth() * phys), H = juce::roundToInt ((float) getHeight() * phys);
        cache = juce::Image (juce::Image::ARGB, juce::jmax (1, W), juce::jmax (1, H), true, juce::SoftwareImageType());
        juce::Graphics s (cache);
        s.addTransform (juce::AffineTransform::scale (phys));

        // contact shadow
        juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.8f), c.x, c.y + d * 0.06f,
                                     juce::Colours::transparentBlack, c.x + d * 0.62f, c.y + d * 0.06f, true);
        s.setGradientFill (shadow);
        s.fillEllipse (area.expanded (d * 0.12f).translated (0.0f, d * 0.06f));

        // bezel ring
        const auto bezel = area.expanded (d * 0.035f);
        s.setGradientFill (juce::ColourGradient (juce::Colour (0xff2d2724), bezel.getX(), bezel.getY(),
                                                 juce::Colour (0xff070605), bezel.getRight(), bezel.getBottom(), false));
        s.fillEllipse (bezel);
        s.setColour (juce::Colour (0xff4a403a).withAlpha (0.6f));
        s.drawEllipse (bezel.reduced (0.5f), 0.8f);

        s.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        s.drawImage (knobBody (juce::roundToInt (d * phys)), area);

        // scale
        for (int i = 0; i < ticks; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
            const bool major = i % 2 == 0;
            const float sx = std::sin (a), sy = -std::cos (a);
            s.setColour ((major ? colours::ash : colours::rim).withAlpha (major ? 0.75f : 0.9f));
            const float r1 = major ? majorR1 : tickR1;
            s.drawLine (c.x + sx * tickR0, c.y + sy * tickR0, c.x + sx * r1, c.y + sy * r1, major ? 1.2f : 0.9f);
            if (showNumbers && major)
            {
                const float rt = d * 0.72f, fh = juce::jmax (8.0f, d * 0.12f);
                s.setFont (fonts::value (fh));
                s.setColour (colours::ash.withAlpha (0.8f));
                s.drawText (juce::String (i / 2), juce::Rectangle<float> (fh * 2.0f, fh).withCentre ({ c.x + sx * rt, c.y + sy * rt }),
                            juce::Justification::centred, false);
            }
        }
    }
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (cache, getLocalBounds().toFloat());

    // value arc from the origin to the value
    const double origin = hasOrigin ? litOrigin : getMinimum();
    const float p0 = (float) valueToProportionOfLength (juce::jlimit (getMinimum(), getMaximum(), origin));
    const float p1 = (float) valueToProportionOfLength (getValue());
    {
        const float a0 = startAngle + (endAngle - startAngle) * juce::jmin (p0, p1);
        const float a1 = startAngle + (endAngle - startAngle) * juce::jmax (p0, p1);
        const float arcR = d * 0.535f;
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (colours::emberDim.withAlpha (0.55f));
        g.strokePath (track, juce::PathStrokeType (juce::jmax (1.2f, d * 0.03f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (a1 - a0 > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
            glowStroke (g, arc, juce::jmax (1.3f, d * 0.032f), colours::ember, 0.65f);
        }
    }

    if (caption.isNotEmpty())
    {
        const auto top = getLocalBounds().toFloat().removeFromTop (captionHeight);
        const bool live = isMouseOverOrDragging();
        if (live)
            glowText (g, getTextFromValue (getValue()), fonts::value (16.0f), top, juce::Justification::centred, colours::emberHot, 0.7f);
        else
            glowText (g, caption, fonts::label (15.0f, 0.1f), top, juce::Justification::centred, colours::bone.withAlpha (0.85f), 0.0f);
    }

    // pointer
    const float angle = startAngle + (endAngle - startAngle) * p1;
    const float sx = std::sin (angle), sy = -std::cos (angle);
    juce::Path pointer;
    pointer.startNewSubPath (c.x + sx * d * 0.08f, c.y + sy * d * 0.08f);
    pointer.lineTo (c.x + sx * d * 0.43f, c.y + sy * d * 0.43f);
    glowStroke (g, pointer, juce::jmax (1.6f, d * 0.04f), colours::ember, isMouseOverOrDragging() ? 1.1f : 0.85f);
    glowSpot (g, { c.x + sx * d * 0.43f, c.y + sy * d * 0.43f }, d * 0.12f, colours::emberHot, 0.5f);

    if (hasKeyboardFocus (false))
    {
        g.setColour (colours::ember.withAlpha (0.4f));
        g.drawEllipse (area.expanded (d * 0.08f), 1.0f);
    }
}

//==============================================================================
GlowButton::GlowButton (const juce::String& text, Style s) : juce::Button (text), style (s)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

GlowButton::GlowButton (Icon i, const juce::String& text, Style s) : juce::Button (text), style (s), icon (i), hasIcon (true)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void GlowButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (1.5f);
    const bool on = getToggleState();
    const float lit = on ? 1.0f : (pulsing ? pulse : 0.0f);
    const float corner = 3.0f;

    if (style != Style::text)
    {
        // body
        g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xff0b0908 : 0xff181412), b.getX(), b.getY(),
                                                 juce::Colour (0xff080605), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, corner);
        if (lit > 0.0f)
        {
            juce::ColourGradient warm (colours::emberDeep.withAlpha (0.38f * lit), b.getCentreX(), b.getBottom(),
                                       juce::Colours::transparentBlack, b.getCentreX(), b.getY(), false);
            g.setGradientFill (warm);
            g.fillRoundedRectangle (b, corner);
        }
        // border: dark outer, lit inner
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (b.expanded (1.0f), corner + 1.0f, 1.2f);
        juce::Path border;
        border.addRoundedRectangle (b, corner);
        if (lit > 0.0f)
            glowStroke (g, border, 1.1f, colours::ember, 0.7f * lit);
        else
        {
            g.setColour (over ? colours::rimLight : colours::rim);
            g.strokePath (border, juce::PathStrokeType (1.1f));
        }
        if (style == Style::plain)
        {
            g.setColour ((lit > 0.0f ? colours::ember : colours::rim).withAlpha (0.6f));
            g.drawRoundedRectangle (b.reduced (3.5f), corner, 0.7f);
        }
    }

    const auto text = getButtonText();
    const auto ink = lit > 0.0f ? colours::emberHot.interpolatedWith (colours::ember, 0.4f)
                                : (over ? colours::bone : (style == Style::plain ? colours::ember.withMultipliedBrightness (0.85f) : colours::ash));
    const float glow = lit > 0.0f ? 0.8f * lit : 0.0f;
    auto content = b.reduced (style == Style::icon ? 3.0f : 6.0f, 2.0f);

    if (style == Style::icon)
    {
        if (text.isEmpty())
        {
            drawIcon (g, icon, content.reduced (content.getHeight() * 0.18f), ink, glow);
            return;
        }
        const auto labelArea = content.removeFromBottom (content.getHeight() * 0.3f);
        drawIcon (g, icon, content.reduced (content.getHeight() * 0.12f), ink, glow);
        glowText (g, text, fonts::label (juce::jmin (11.0f, labelArea.getHeight() * 0.95f), 0.08f), labelArea,
                  juce::Justification::centred, ink, glow * 0.6f);
        return;
    }

    const float fh = juce::jmin (style == Style::plain ? 17.0f : 14.0f, b.getHeight() * 0.48f);
    const auto font = style == Style::plain ? fonts::serif (fh, 0.18f) : fonts::label (fh, 0.08f);
    if (hasIcon)
    {
        const float tw = juce::GlyphArrangement::getStringWidth (font, text);
        const float iconSize = b.getHeight() * 0.56f;
        const float total = iconSize + 8.0f + tw;
        auto row = juce::Rectangle<float> (total, b.getHeight()).withCentre (b.getCentre());
        drawIcon (g, icon, row.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize), ink, glow);
        row.removeFromLeft (8.0f);
        glowText (g, text, font, row, juce::Justification::centredLeft, ink, glow);
        return;
    }
    glowText (g, text, font, content, juce::Justification::centred, ink, glow);
}

//==============================================================================
ChainBlock::ChainBlock (const juce::String& n, Icon i) : name (n), icon (i)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ChainBlock::setSelected (bool s)       { if (selected != s) { selected = s; repaint(); } }
void ChainBlock::setEnabledState (bool e)   { if (enabledState != e) { enabledState = e; repaint(); } }
void ChainBlock::setActivity (float level)
{
    level = juce::jlimit (0.0f, 1.0f, level);
    if (std::abs (level - activity) > 0.02f) { activity = level; repaint(); }
}

juce::Rectangle<float> ChainBlock::barArea() const
{
    const auto b = getLocalBounds().toFloat();
    return { b.getCentreX() - 22.0f, b.getBottom() - 15.0f, 44.0f, 12.0f };
}

void ChainBlock::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;
    if (barArea().expanded (8.0f, 4.0f).contains (e.position) && onToggle)
        onToggle();
    else if (onSelect)
        onSelect();
}

void ChainBlock::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (3.0f);
    const bool over = isMouseOver (true);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff120e0c), b.getX(), b.getY(), juce::Colour (0xff070504), b.getX(), b.getBottom(), false));
    g.fillRect (b);
    if (selected)
    {
        juce::ColourGradient warm (colours::emberDeep.withAlpha (0.35f), b.getCentreX(), b.getBottom(),
                                   juce::Colours::transparentBlack, b.getCentreX(), b.getY() + b.getHeight() * 0.3f, false);
        g.setGradientFill (warm);
        g.fillRect (b);
    }

    juce::Path frame;
    frame.addRectangle (b);
    if (selected)
        glowStroke (g, frame, 1.6f, colours::ember, 1.0f, juce::PathStrokeType::mitered);
    else
    {
        g.setColour (juce::Colours::black);
        g.strokePath (frame, juce::PathStrokeType (3.0f));
        g.setColour (over ? colours::rimLight : colours::rim);
        g.strokePath (frame, juce::PathStrokeType (1.0f));
        g.setColour (colours::rim.withAlpha (0.5f));
        g.drawRect (b.reduced (3.0f), 0.7f);
    }

    const auto ink = selected ? colours::emberHot : (over ? colours::bone : colours::ash);
    glowText (g, name, fonts::label (14.0f, 0.12f), b.withHeight (22.0f).translated (0.0f, 3.0f), juce::Justification::centred,
              ink, selected ? 0.7f : 0.0f);

    const float warmth = juce::jmax (selected ? 0.9f : 0.0f, enabledState ? activity : 0.0f);
    const auto iconArea = juce::Rectangle<float> (30.0f, 30.0f).withCentre ({ b.getCentreX(), b.getCentreY() + 2.0f });
    drawIcon (g, icon, iconArea, enabledState ? (warmth > 0.05f ? colours::ember.interpolatedWith (colours::emberHot, warmth * 0.5f) : colours::bone)
                                              : colours::rim, enabledState ? warmth : 0.0f);

    // enable bar
    const auto bar = barArea().withSizeKeepingCentre (barArea().getWidth(), 4.0f);
    if (enabledState)
    {
        glowSpot (g, bar.getCentre(), 26.0f, colours::ember, 0.45f);
        g.setColour (colours::emberHot);
        g.fillRoundedRectangle (bar, 2.0f);
    }
    else
    {
        g.setColour (colours::emberDim.withAlpha (0.8f));
        g.fillRoundedRectangle (bar, 2.0f);
    }
}

//==============================================================================
void ChainLink::setLevel (float l)
{
    level = juce::jlimit (0.0f, 1.0f, l);
    ++frame;
    repaint();
}

void ChainLink::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float cy = b.getCentreY();
    std::uint32_t state = (std::uint32_t) frame * 2654435761u + 17u;

    // the dormant vein
    g.setColour (colours::emberDim);
    g.drawLine (b.getX(), cy, b.getRight(), cy, 1.2f);

    const float energy = 0.25f + 0.75f * level;
    juce::Path bolt;
    bolt.startNewSubPath (b.getX(), cy);
    const int segments = juce::jmax (4, (int) (b.getWidth() / 5.0f));
    for (int i = 1; i < segments; ++i)
    {
        const float x = b.getX() + b.getWidth() * (float) i / (float) segments;
        const float envelope = std::sin (pi * (float) i / (float) segments);
        bolt.lineTo (x, cy + (rand01 (state) - 0.5f) * 2.0f * (1.5f + 9.0f * level) * envelope);
    }
    bolt.lineTo (b.getRight(), cy);
    glowStroke (g, bolt, 1.1f, colours::ember, energy, juce::PathStrokeType::mitered);

    if (level > 0.2f)
        for (int i = 0; i < 3; ++i)
            glowSpot (g, { b.getX() + b.getWidth() * rand01 (state), cy + (rand01 (state) - 0.5f) * 10.0f * level },
                      3.0f + 5.0f * level, colours::emberHot, 0.6f * level);
}

//==============================================================================
Jack::Jack (const juce::String& name) : juce::Button (name)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Jack::paintButton (juce::Graphics& g, bool over, bool)
{
    auto b = getLocalBounds().toFloat();
    glowText (g, getButtonText(), fonts::label (14.0f, 0.12f), b.removeFromTop (20.0f), juce::Justification::centred,
              over ? colours::bone : colours::ash, 0.0f);
    const auto c = b.getCentre();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.36f;
    const bool on = getToggleState() || ! getClickingTogglesState();
    const float heat = on ? 0.45f + 0.55f * level : 0.0f;

    glowSpot (g, c, r * 2.2f, colours::ember, 0.35f * heat);
    g.setColour (juce::Colour (0xff0a0807));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    juce::Path rings;
    rings.addEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    rings.addEllipse (juce::Rectangle<float> (r * 1.25f, r * 1.25f).withCentre (c));
    if (heat > 0.0f)
        glowStroke (g, rings, 1.6f, colours::ember, heat);
    else
    {
        g.setColour (colours::rim);
        g.strokePath (rings, juce::PathStrokeType (1.4f));
    }
    glowSpot (g, c, r * 0.5f, colours::emberHot, heat);
}

//==============================================================================
VMeter::VMeter (float lo, float hi, std::vector<float> scaleMarks, bool showReadout)
    : minDb (lo), maxDb (hi), marks (std::move (scaleMarks)), readout (showReadout)
{
    setInterceptsMouseClicks (false, false);
}

void VMeter::pushLevel (float db)
{
    level = db > level ? db : juce::jmax (db, level - 1.2f);
    if (db >= hold) { hold = db; holdFrames = 45; }
    else if (--holdFrames < 0) hold -= 0.8f;
    shown = shown < -99.0f ? db : shown + 0.12f * (juce::jmax (db, -99.0f) - shown);
    repaint();
}

void VMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    juce::Rectangle<float> text;
    if (readout)
        text = b.removeFromBottom (16.0f);
    juce::Rectangle<float> scale;
    if (! marks.empty())
        scale = b.removeFromRight (26.0f);
    b = b.reduced (1.0f, 2.0f);
    b = b.withSizeKeepingCentre (juce::jmin (b.getWidth(), 14.0f), b.getHeight());
    if (! scale.isEmpty())
        b.setX (scale.getX() - b.getWidth() - 6.0f);

    auto toY = [&] (float db) { return b.getBottom() - b.getHeight() * juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb)); };

    g.setColour (juce::Colour (0xff050403));
    g.fillRect (b.expanded (1.0f));
    g.setColour (colours::rim.withAlpha (0.8f));
    g.drawRect (b.expanded (1.0f), 0.8f);

    const float levelY = toY (level);
    const float step = 2.6f;
    for (float y = b.getBottom() - 1.0f; y > b.getY(); y -= step)
    {
        const float t = (b.getBottom() - y) / b.getHeight();
        const bool lit = y >= levelY;
        g.setColour (lit ? emberRamp (0.25f + 0.7f * t) : colours::emberDim.withAlpha (0.28f));
        const float half = (b.getWidth() - 3.0f) * 0.5f;
        g.fillRect (b.getX() + 1.0f, y - 1.0f, half, 1.2f);          // twin columns
        g.fillRect (b.getX() + 2.0f + half, y - 1.0f, half, 1.2f);
    }
    if (level > minDb)
        glowSpot (g, { b.getCentreX(), levelY }, b.getWidth() * 1.2f, colours::ember, 0.35f);
    if (hold > minDb)
    {
        g.setColour (colours::emberHot);
        g.fillRect (b.getX(), toY (hold) - 1.0f, b.getWidth(), 1.6f);
    }
    if (targetMark)
    {
        const float y = toY (0.0f);
        g.setColour (colours::bone.withAlpha (0.8f));
        g.drawLine (b.getX() - 3.0f, y, b.getRight() + 3.0f, y, 1.0f);
    }

    if (! scale.isEmpty())
    {
        g.setFont (fonts::value (11.0f));
        for (float m : marks)
        {
            const float y = toY (m);
            g.setColour (colours::ash);
            const auto txt = m > 0.0f ? "+" + juce::String ((int) m) : juce::String ((int) m).replace ("-", juce::String::charToString (0x2212));
            g.drawText (txt, juce::Rectangle<float> (scale.getX() + 3.0f, y - 6.0f, scale.getWidth() - 3.0f, 12.0f), juce::Justification::centredLeft, false);
        }
    }
    if (readout)
    {
        const auto txt = shown <= minDb + 0.5f ? juce::String::charToString (0x2212) + juce::String::charToString (0x221e)
                                              : (juce::String (shown, 1) + " dB").replace ("-", juce::String::charToString (0x2212));
        g.setFont (fonts::value (12.0f));
        g.setColour (colours::bone.withAlpha (0.85f));
        g.drawText (txt, text, juce::Justification::centred, false);
    }
}

//==============================================================================
void Radar::push (float levelDb, float targetDb, bool playing)
{
    sweep = std::fmod (sweep + 0.07f, 2.0f * pi);
    for (auto& blip : blips)
        blip.life -= 0.025f;
    blips.erase (std::remove_if (blips.begin(), blips.end(), [] (const Blip& b) { return b.life <= 0.0f; }), blips.end());

    const float error = std::abs (levelDb - targetDb);
    if (playing)
    {
        blips.push_back ({ sweep, juce::jlimit (0.0f, 1.0f, error / 18.0f), 1.0f });
        centreGlow = centreGlow * 0.85f + 0.15f * juce::jlimit (0.0f, 1.0f, 1.0f - error / 6.0f);
    }
    else
        centreGlow *= 0.94f;
    if (blips.size() > 160)
        blips.erase (blips.begin(), blips.begin() + (long) (blips.size() - 160));
    repaint();
}

void Radar::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.46f;

    g.setColour (juce::Colour (0xff050403).withAlpha (0.8f));
    g.fillEllipse (juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));

    // rings at 6 / 12 / 18 dB off target, cross-hair, bearing ticks
    g.setColour (colours::rim.withAlpha (0.9f));
    for (float k : { 1.0f / 3.0f, 2.0f / 3.0f, 1.0f })
        g.drawEllipse (juce::Rectangle<float> (R * 2.0f * k, R * 2.0f * k).withCentre (c), k == 1.0f ? 1.2f : 0.7f);
    for (int i = 0; i < 8; ++i)
    {
        const float a = (float) i * pi * 0.25f;
        g.drawLine (c.x, c.y, c.x + std::cos (a) * R, c.y + std::sin (a) * R, i % 2 == 0 ? 0.7f : 0.4f);
    }
    for (int i = 0; i < 72; ++i)
    {
        const float a = (float) i * pi / 36.0f, len = i % 6 == 0 ? 6.0f : 3.0f;
        g.drawLine (c.x + std::cos (a) * R, c.y + std::sin (a) * R, c.x + std::cos (a) * (R + len), c.y + std::sin (a) * (R + len), 0.7f);
    }
    g.setFont (fonts::value (10.0f));
    g.setColour (colours::ash.withAlpha (0.8f));
    for (int k = 1; k <= 3; ++k)
        g.drawText (juce::String (k * 6), juce::Rectangle<float> (20.0f, 10.0f).withCentre ({ c.x + 9.0f, c.y - R * (float) k / 3.0f + 6.0f }),
                    juce::Justification::centredLeft, false);

    // sweep wedge
    {
        const juce::Graphics::ScopedSaveState s (g);
        for (int i = 0; i < 14; ++i)
        {
            const float a0 = sweep - (float) i * 0.06f;
            juce::Path wedge;
            wedge.startNewSubPath (c);
            wedge.lineTo (c.x + std::cos (a0) * R, c.y + std::sin (a0) * R);
            wedge.lineTo (c.x + std::cos (a0 - 0.06f) * R, c.y + std::sin (a0 - 0.06f) * R);
            wedge.closeSubPath();
            g.setColour (colours::ember.withAlpha (0.12f * (1.0f - (float) i / 14.0f)));
            g.fillPath (wedge);
        }
        juce::Path line;
        line.startNewSubPath (c);
        line.lineTo (c.x + std::cos (sweep) * R, c.y + std::sin (sweep) * R);
        glowStroke (g, line, 0.8f, colours::ember, 0.5f);
    }

    for (const auto& blip : blips)
    {
        const auto p = c + juce::Point<float> (std::cos (blip.angle), std::sin (blip.angle)) * (blip.radius * R);
        glowSpot (g, p, 5.0f, colours::ember, 0.8f * blip.life);
        g.setColour (colours::emberHot.withAlpha (blip.life));
        g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre (p));
    }

    // the target
    glowSpot (g, c, R * (0.25f + 0.25f * centreGlow), colours::ember, 0.25f + 0.6f * centreGlow);
    juce::Path star;
    const float s = R * (0.06f + 0.08f * centreGlow);
    star.startNewSubPath (c.x - s * 2.0f, c.y); star.lineTo (c.x + s * 2.0f, c.y);
    star.startNewSubPath (c.x, c.y - s * 2.0f); star.lineTo (c.x, c.y + s * 2.0f);
    glowStroke (g, star, 1.0f, colours::emberHot, 0.4f + 0.6f * centreGlow);
}

//==============================================================================
void Scope::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (colours::rim.withAlpha (0.4f));
    g.drawLine (b.getX(), b.getCentreY(), b.getRight(), b.getCentreY(), 0.6f);
    if (samples.size() < 2)
        return;

    float peak = 1.0e-4f;
    for (float s : samples) peak = juce::jmax (peak, std::abs (s));
    const float gain = 0.45f * b.getHeight() / juce::jmax (0.05f, peak);   // auto-scaled, gentle
    juce::Path trace;
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const float x = b.getX() + b.getWidth() * (float) i / (float) (samples.size() - 1);
        const float y = b.getCentreY() - juce::jlimit (-0.48f * b.getHeight(), 0.48f * b.getHeight(), samples[i] * gain);
        if (i == 0) trace.startNewSubPath (x, y); else trace.lineTo (x, y);
    }
    glowStroke (g, trace, 0.9f, colours::ember, juce::jlimit (0.2f, 0.9f, peak * 3.0f), juce::PathStrokeType::mitered);
}

//==============================================================================
void Gauge::setValue (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (std::abs (v - value) > 0.003f) { value = v; repaint(); }
}

void Gauge::paint (juce::Graphics& g)
{
    const auto b = centredSquare (getLocalBounds().toFloat().reduced (2.0f));
    const auto c = b.getCentre();
    const float R = b.getWidth() * 0.5f;
    g.setColour (juce::Colour (0xff060504));
    g.fillEllipse (b);
    g.setColour (colours::rim);
    g.drawEllipse (b, 1.2f);
    const float a0 = -0.75f * pi, a1 = 0.75f * pi;
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / 10.0f;
        const float sx = std::sin (a), sy = -std::cos (a);
        g.setColour ((float) i / 10.0f <= value ? colours::ember : colours::rim);
        g.drawLine (c.x + sx * R * 0.72f, c.y + sy * R * 0.72f, c.x + sx * R * 0.88f, c.y + sy * R * 0.88f, i % 5 == 0 ? 1.4f : 0.8f);
    }
    const float a = a0 + (a1 - a0) * value;
    juce::Path needle;
    needle.startNewSubPath (c);
    needle.lineTo (c.x + std::sin (a) * R * 0.8f, c.y - std::cos (a) * R * 0.8f);
    glowStroke (g, needle, 1.2f, colours::ember, 0.8f);
    g.setColour (colours::rimLight);
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (c));
}

//==============================================================================
void HBar::setValue (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (std::abs (v - value) > 0.003f) { value = v; repaint(); }
}

void HBar::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float step = 3.0f;
    const int n = (int) (b.getWidth() / step);
    const int lit = juce::roundToInt (value * (float) n);
    for (int i = 0; i < n; ++i)
    {
        const auto seg = juce::Rectangle<float> (b.getX() + (float) i * step, b.getY(), 1.8f, b.getHeight());
        g.setColour (i < lit ? emberRamp (0.35f + 0.6f * (float) i / (float) n) : colours::emberDim.withAlpha (0.45f));
        g.fillRect (seg);
    }
    if (lit > 0)
        glowSpot (g, { b.getX() + (float) lit * step, b.getCentreY() }, b.getHeight() * 2.5f, colours::ember, 0.35f);
}

//==============================================================================
void Portal::setIntensity (float v)
{
    intensity = intensity * 0.7f + 0.3f * juce::jlimit (0.0f, 1.0f, v);
    phase += 0.04f;
    repaint();
}

void Portal::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float phys = physicalScale (g);
    if (cache.isNull() || std::abs (cacheScale - phys) > 0.01f)
    {
        cacheScale = phys;
        const int W = juce::jmax (1, juce::roundToInt (b.getWidth() * phys)), H = juce::jmax (1, juce::roundToInt (b.getHeight() * phys));
        // two layers side by side: cold rock, and the molten cracks alone
        cache = juce::Image (juce::Image::ARGB, W * 2, H, true, juce::SoftwareImageType());
        juce::Image::BitmapData bd (cache, juce::Image::BitmapData::writeOnly);
        const float inner = style == Style::chamber ? 0.56f : 0.72f;
        const float cell = style == Style::chamber ? 0.11f : 0.16f;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float nx = ((float) x + 0.5f) / (float) W * 2.0f - 1.0f, ny = ((float) y + 0.5f) / (float) H * 2.0f - 1.0f;
                const float r = std::sqrt (nx * nx + ny * ny);
                if (r > 1.0f)
                    continue;
                const float edge = juce::jlimit (0.0f, 1.0f, (1.0f - r) * (float) W * 0.5f);
                if (r < inner)
                {
                    const float v = 0.02f + 0.03f * (r / inner);
                    bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (v * 1.1f, v * 0.8f, v * 0.7f, 1.0f));
                    continue;
                }
                // cells
                const float u = nx / cell, w = ny / cell;
                const int ix = (int) std::floor (u), iy = (int) std::floor (w);
                float d1 = 9.0f, d2 = 9.0f;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        std::uint32_t st = (std::uint32_t) ((ix + dx) * 73856093) ^ (std::uint32_t) ((iy + dy) * 19349663);
                        const float px = (float) (ix + dx) + 0.15f + 0.7f * rand01 (st);
                        const float py = (float) (iy + dy) + 0.15f + 0.7f * rand01 (st);
                        const float d = (px - u) * (px - u) + (py - w) * (py - w);
                        if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
                    }
                const float gap = std::sqrt (d2) - std::sqrt (d1);
                const float crack = 1.0f - juce::jlimit (0.0f, 1.0f, gap / 0.12f);
                const float band = (r - inner) / (1.0f - inner);           // 0 at the inner rim
                const float heat = juce::jlimit (0.0f, 1.0f, 1.15f - band * 1.3f);
                const float rock = (0.05f + 0.06f * (1.0f - crack)) * (0.6f + 0.4f * (1.0f - band));
                bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (rock * 1.15f, rock * 0.95f, rock * 0.85f, edge));
                const float molten = crack * heat;
                const auto hot = emberRamp (0.35f + 0.6f * molten);
                bd.setPixelColour (W + x, y, hot.withAlpha (juce::jlimit (0.0f, 1.0f, molten * 1.3f) * edge));
            }
    }

    const int W = cache.getWidth() / 2;
    const float breathe = 0.85f + 0.15f * std::sin (phase);
    const float heat = juce::jlimit (0.0f, 1.0f, (0.35f + 0.65f * intensity) * breathe);
    const auto c = b.getCentre();
    glowSpot (g, c, b.getWidth() * 0.62f, colours::ember, 0.22f * heat);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (cache, b.getX(), b.getY(), b.getWidth(), b.getHeight(), 0, 0, W, cache.getHeight());
    g.setOpacity (heat);
    g.drawImage (cache, b.getX(), b.getY(), b.getWidth(), b.getHeight(), W, 0, W, cache.getHeight());
    g.setOpacity (1.0f);

    const float inner = style == Style::chamber ? 0.56f : 0.72f;
    juce::Path rim;
    rim.addEllipse (b.withSizeKeepingCentre (b.getWidth() * inner, b.getHeight() * inner));
    glowStroke (g, rim, 1.2f, colours::ember, 0.5f * heat);
    if (style == Style::chamber)
    {
        glowSpot (g, c, b.getWidth() * 0.2f, colours::emberDeep, 0.5f * heat);
        drawIcon (g, Icon::trident, b.withSizeKeepingCentre (b.getWidth() * 0.26f, b.getWidth() * 0.26f), colours::ember, 0.6f + 0.6f * heat);
    }
}

//==============================================================================
LinearSlider::LinearSlider()
{
    setSliderStyle (juce::Slider::LinearHorizontal);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setVelocityModeParameters (0.12, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void LinearSlider::setPopupParent (juce::Component* parent)
{
    setPopupDisplayEnabled (true, true, parent, 1200);
}

void LinearSlider::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (8.0f, 0.0f);
    const float y = b.getCentreY();
    const float p = (float) valueToProportionOfLength (getValue());
    const float x = b.getX() + b.getWidth() * p;

    g.setColour (juce::Colour (0xff050403));
    g.fillRoundedRectangle (b.withSizeKeepingCentre (b.getWidth(), 4.0f), 2.0f);
    g.setColour (colours::rim);
    g.drawRoundedRectangle (b.withSizeKeepingCentre (b.getWidth(), 4.0f), 2.0f, 0.8f);
    for (int i = 0; i <= 10; ++i)
    {
        const float tx = b.getX() + b.getWidth() * (float) i / 10.0f;
        g.setColour (colours::rim);
        g.drawLine (tx, y + 5.0f, tx, y + (i % 5 == 0 ? 10.0f : 8.0f), 0.8f);
    }
    juce::Path fill;
    fill.startNewSubPath (b.getX(), y);
    fill.lineTo (x, y);
    glowStroke (g, fill, 1.6f, colours::ember, 0.6f);
    glowSpot (g, { x, y }, 12.0f, colours::ember, 0.7f);
    g.setColour (colours::emberHot);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x, y }));
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.fillEllipse (juce::Rectangle<float> (2.5f, 2.5f).withCentre ({ x - 0.8f, y - 0.8f }));
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff0c0908));
    setColour (juce::PopupMenu::textColourId,                  colours::bone);
    setColour (juce::PopupMenu::headerTextColourId,            colours::ash);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::emberDeep.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId,       colours::emberHot);

    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf50b0807));
    setColour (juce::TooltipWindow::textColourId,       colours::bone);
    setColour (juce::TooltipWindow::outlineColourId,    colours::rim);

    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff0d0a09));
    setColour (juce::AlertWindow::textColourId,       colours::bone);
    setColour (juce::AlertWindow::outlineColourId,    colours::rim);

    setColour (juce::TextEditor::backgroundColourId,      juce::Colour (0xff060403));
    setColour (juce::TextEditor::textColourId,            colours::emberHot);
    setColour (juce::TextEditor::outlineColourId,         colours::rim);
    setColour (juce::TextEditor::focusedOutlineColourId,  colours::ember.withAlpha (0.8f));
    setColour (juce::TextEditor::highlightColourId,       colours::ember.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId,       colours::ember);

    setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff141010));
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2a120a));
    setColour (juce::TextButton::textColourOffId,  colours::ash);
    setColour (juce::TextButton::textColourOnId,   colours::emberHot);
    setColour (juce::ResizableWindow::backgroundColourId, colours::coal);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto b = juce::Rectangle<float> ((float) width, (float) height);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff120d0b), 0.0f, 0.0f, juce::Colour (0xff070504), 0.0f, b.getBottom(), false));
    g.fillRect (b);
    g.setColour (colours::rim);
    g.drawRect (b, 1.0f);
    g.setColour (colours::ember.withAlpha (0.6f));
    g.fillRect (b.withHeight (1.0f).reduced (8.0f, 0.0f));
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                     bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                     const juce::String& shortcutKeyText, const juce::Drawable*, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (colours::rim);
        g.fillRect (area.reduced (12, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (4, 1).toFloat();
    if (isHighlighted && isActive)
    {
        juce::ColourGradient warm (colours::emberDeep.withAlpha (0.5f), r.getX(), 0.0f, colours::emberDeep.withAlpha (0.0f), r.getRight(), 0.0f, false);
        g.setGradientFill (warm);
        g.fillRect (r);
        g.setColour (colours::ember);
        g.fillRect (r.withWidth (2.0f));
    }
    if (isTicked)
    {
        const juce::Point<float> c (r.getX() + 13.0f, r.getCentreY());
        glowSpot (g, c, 9.0f, colours::ember, 0.7f);
        g.setColour (colours::emberHot);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (c));
    }
    const auto ink = textColour != nullptr ? *textColour
                   : (! isActive ? colours::rim : (isHighlighted ? colours::emberHot : (isTicked ? colours::ember : colours::bone)));
    g.setColour (ink);
    g.setFont (getPopupMenuFont());
    const auto textArea = r.withTrimmedLeft (26.0f).withTrimmedRight (14.0f);
    g.drawFittedText (text, textArea.toNearestInt(), juce::Justification::centredLeft, 1);
    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (colours::ash);
        g.setFont (fonts::value (12.0f));
        g.drawText (shortcutKeyText, textArea, juce::Justification::centredRight);
    }
    if (hasSubMenu)
        drawIcon (g, Icon::next, juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ r.getRight() - 10.0f, r.getCentreY() }), colours::ash, 0.0f);
}

void LookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setFont (fonts::serif (11.0f, 0.35f));
    g.setColour (colours::ember.withAlpha (0.8f));
    g.drawFittedText (name.toUpperCase(), area.withTrimmedLeft (14).withTrimmedBottom (2), juce::Justification::bottomLeft, 1);
}

juce::Font LookAndFeel::getPopupMenuFont() { return fonts::label (16.0f, 0.04f); }

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto b = juce::Rectangle<float> ((float) width, (float) height);
    g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
    g.fillRect (b);
    g.setColour (colours::rim);
    g.drawRect (b, 1.0f);
    g.setColour (colours::ember.withAlpha (0.7f));
    g.fillRect (b.withWidth (2.0f));
    g.setColour (colours::bone);
    g.setFont (Fonts::body (14.0f));
    g.drawFittedText (text, b.reduced (10.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 4);
}

juce::Font LookAndFeel::getSliderPopupFont (juce::Slider&) { return fonts::value (15.0f); }

void LookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    const auto b = body.reduced (0.5f);
    g.setColour (juce::Colour (0xf2080605));
    g.fillRoundedRectangle (b, 3.0f);
    juce::Path border;
    border.addRoundedRectangle (b, 3.0f);
    glowStroke (g, border, 0.8f, colours::ember, 0.4f);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return fonts::labelBold (juce::jmin (15.0f, (float) buttonHeight * 0.5f), 0.14f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down)
{
    auto b = button.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = button.getToggleState();
    g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xff0b0908 : 0xff181412), b.getX(), b.getY(),
                                             juce::Colour (0xff080605), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, 3.0f);
    juce::Path border;
    border.addRoundedRectangle (b, 3.0f);
    if (on)
        glowStroke (g, border, 1.0f, colours::ember, 0.6f);
    else
    {
        g.setColour (over ? colours::rimLight : colours::rim);
        g.strokePath (border, juce::PathStrokeType (1.0f));
    }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool over, bool)
{
    const bool on = button.getToggleState();
    glowText (g, button.getButtonText(), getTextButtonFont (button, button.getHeight()), button.getLocalBounds().toFloat(),
              juce::Justification::centred,
              (on ? colours::emberHot : (over ? colours::bone : colours::ash)).withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f),
              on ? 0.6f : 0.0f);
}

juce::Font LookAndFeel::getAlertWindowTitleFont()   { return fonts::serif (19.0f, 0.3f); }
juce::Font LookAndFeel::getAlertWindowMessageFont() { return Fonts::body (16.0f); }
juce::Font LookAndFeel::getAlertWindowFont()        { return Fonts::body (15.0f); }

TunerTheme tunerTheme()
{
    TunerTheme t;
    t.accent = colours::ember;
    t.inTune = colours::emberHot;
    t.text = colours::bone;
    t.dim = colours::ash;
    t.faint = colours::emberDim;
    t.panelTop = juce::Colour (0xff140f0d);
    t.panelBottom = juce::Colour (0xff060404);
    t.outline = colours::rim;
    t.noteFont = [] (float h) { return fonts::serif (h * 0.85f, 0.0f); };
    t.labelFont = [] (float h) { return fonts::serif (h, 0.4f); };
    t.readoutFont = [] (float h) { return fonts::value (h + 1.0f); };
    return t;
}

} // namespace apex::ui::abyss
