#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    Professional dark/amber look for ApexAmp.

    Goes for a "hardware" feel rather than a flat/web look: knobs have a recessed
    shadow, a brushed-metal body gradient, tick marks, and a glowing amber value
    arc; combo boxes and buttons are rounded with subtle borders and hover states.
*/
class ApexLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ApexLookAndFeel()
    {
        using juce::Colour;
        accent   = Colour (0xffff7a18);   // amber
        accentHi = Colour (0xffffa54d);
        bg       = Colour (0xff14161b);
        panel    = Colour (0xff20242c);
        panelHi  = Colour (0xff2a2f39);
        line     = Colour (0xff0e0f13);
        text     = juce::Colours::white.withAlpha (0.88f);

        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::arrowColourId, accent);

        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.30f));
        setColour (juce::PopupMenu::textColourId, text);

        setColour (juce::TextButton::buttonColourId, panelHi);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::Label::textColourId, text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        using namespace juce;
        auto area = Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float radius = jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
        const auto  c = area.getCentre();
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        // recessed shadow
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - radius - 1.5f, c.y - radius + 2.0f, (radius + 1.5f) * 2.0f, (radius + 1.5f) * 2.0f);

        // tick marks around the dial
        const int ticks = 11;
        for (int i = 0; i < ticks; ++i)
        {
            const float t = (float) i / (float) (ticks - 1);
            const float a = startAngle + t * (endAngle - startAngle);
            const float r0 = radius + 2.5f, r1 = radius + 5.0f;
            const Point<float> p0 (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0);
            const Point<float> p1 (c.x + std::sin (a) * r1, c.y - std::cos (a) * r1);
            g.setColour (Colours::white.withAlpha (t <= sliderPos ? 0.55f : 0.15f));
            g.drawLine ({ p0, p1 }, 1.4f);
        }

        // body with brushed-metal vertical gradient
        ColourGradient grad (panelHi.brighter (0.18f), c.x, c.y - radius,
                             panel.darker (0.35f), c.x, c.y + radius, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (line);
        g.drawEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // value track + arc
        const float arcR = radius + 0.5f;
        Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

        Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (val, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded)); // glow
        g.setColour (accent);
        g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

        // pointer
        Path ptr;
        const float len = radius * 0.66f;
        ptr.addRoundedRectangle (-1.7f, -len, 3.4f, len, 1.6f);
        ptr.applyTransform (AffineTransform::rotation (angle).translated (c.x, c.y));
        g.setColour (Colours::white.withAlpha (0.95f));
        g.fillPath (ptr);

        // centre cap
        g.setColour (panelHi.brighter (0.05f));
        g.fillEllipse (c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f);
        g.setColour (line);
        g.drawEllipse (c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f, 1.0f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0, 0, (float) width, (float) height).reduced (1.0f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 5.0f, 1.2f);

        const float az = 5.0f;
        const float ax = (float) width - 16.0f, ay = (float) height * 0.5f;
        Path arrow;
        arrow.addTriangle (ax - az, ay - az * 0.5f, ax + az, ay - az * 0.5f, ax, ay + az * 0.6f);
        g.setColour (accent);
        g.fillPath (arrow);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg_,
                               bool highlighted, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto col = bg_;
        if (down)        col = col.brighter (0.2f);
        else if (highlighted) col = col.brighter (0.1f);
        g.setColour (col);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        return juce::Font (l.getFont().getHeight(), juce::Font::bold);
    }

    juce::Colour accent, accentHi, bg, panel, panelHi, line, text;
};
