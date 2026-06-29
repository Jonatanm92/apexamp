#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    Retail-grade dark/amber look for ApexAmp.

    Aims for a real "hardware unit" feel: brushed-metal panels with bevels and
    drop shadows, metallic knobs with a radial highlight + glowing amber indicator,
    LED-style toggles, and chassis screws — not a flat web look.
*/
class ApexLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ApexLookAndFeel()
    {
        using juce::Colour;
        accent   = Colour (0xffff7a18);
        accentHi = Colour (0xffffb259);
        bg       = Colour (0xff111319);
        panel    = Colour (0xff262b34);
        panelHi  = Colour (0xff333a45);
        line     = Colour (0xff0c0d11);
        text     = juce::Colours::white.withAlpha (0.9f);

        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

        setColour (juce::ComboBox::backgroundColourId, Colour (0xff1b1f26));
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::arrowColourId, accent);

        setColour (juce::PopupMenu::backgroundColourId, Colour (0xff1b1f26));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.32f));
        setColour (juce::PopupMenu::textColourId, text);

        setColour (juce::TextButton::buttonColourId, panelHi);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::Label::textColourId, text);
    }

    // ---- chassis screw ----------------------------------------------------
    static void drawScrew (juce::Graphics& g, float cx, float cy, float r)
    {
        using namespace juce;
        ColourGradient sg (Colour (0xff545a66), cx - r, cy - r, Colour (0xff181a1f), cx + r, cy + r, false);
        g.setGradientFill (sg);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour (Colour (0xff0b0c0f));
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
        g.setColour (Colour (0xff0b0c0f).withAlpha (0.85f));
        const float s = r * 0.62f;
        g.drawLine (cx - s, cy - s, cx + s, cy + s, 1.3f);  // phillips slot
        g.drawLine (cx - s, cy + s, cx + s, cy - s, 1.3f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillEllipse (cx - r * 0.5f, cy - r * 0.6f, r * 0.5f, r * 0.4f); // highlight
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

        // drop shadow
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - radius, c.y - radius + 3.0f, radius * 2.0f, radius * 2.0f);

        // tick marks
        const int ticks = 11;
        for (int i = 0; i < ticks; ++i)
        {
            const float t = (float) i / (float) (ticks - 1);
            const float a = startAngle + t * (endAngle - startAngle);
            const float r0 = radius + 3.0f, r1 = radius + 6.0f;
            g.setColour (Colours::white.withAlpha (t <= sliderPos ? 0.6f : 0.14f));
            g.drawLine ({ c.x + std::sin (a) * r0, c.y - std::cos (a) * r0,
                          c.x + std::sin (a) * r1, c.y - std::cos (a) * r1 }, 1.4f);
        }

        // metal bezel ring
        ColourGradient bezel (panelHi.brighter (0.35f), c.x, c.y - radius,
                              line, c.x, c.y + radius, false);
        g.setGradientFill (bezel);
        g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);

        // inner body with a radial highlight (top-left light source)
        const float br = radius * 0.82f;
        ColourGradient body (Colour (0xff3a4049), c.x - br * 0.35f, c.y - br * 0.45f,
                             Colour (0xff15171c), c.x, c.y + br, true);
        g.setGradientFill (body);
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (line);
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

        // value track + glowing arc
        const float arcR = radius + 1.0f;
        Path track; track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        Path val; val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (val, PathStrokeType (6.5f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (accentHi);
        g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

        // indicator line + glowing dot at the rim
        const Point<float> tip (c.x + std::sin (angle) * (br - 4.0f),
                                c.y - std::cos (angle) * (br - 4.0f));
        g.setColour (Colours::white.withAlpha (0.92f));
        g.drawLine ({ c.x + std::sin (angle) * (br * 0.35f), c.y - std::cos (angle) * (br * 0.35f), tip.x, tip.y }, 2.6f);
        g.setColour (accent.withAlpha (0.35f));
        g.fillEllipse (tip.x - 4.0f, tip.y - 4.0f, 8.0f, 8.0f);
        g.setColour (accentHi);
        g.fillEllipse (tip.x - 2.2f, tip.y - 2.2f, 4.4f, 4.4f);

        // centre cap
        ColourGradient cap (Colour (0xff444b55), c.x, c.y - 6.0f, Colour (0xff202329), c.x, c.y + 6.0f, false);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
        g.setColour (line);
        g.drawEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f, 1.0f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0, 0, (float) width, (float) height).reduced (1.0f);
        ColourGradient cg (Colour (0xff242932), 0, 0, Colour (0xff171b21), 0, (float) height, false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawLine (r.getX() + 4, r.getY() + 1.5f, r.getRight() - 4, r.getY() + 1.5f, 1.0f); // top sheen
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 5.0f, 1.2f);

        const float az = 5.0f, ax = (float) width - 16.0f, ay = (float) height * 0.5f;
        Path arrow; arrow.addTriangle (ax - az, ay - az * 0.5f, ax + az, ay - az * 0.5f, ax, ay + az * 0.6f);
        g.setColour (accent);
        g.fillPath (arrow);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc,
                               bool highlighted, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        Colour top = on ? accentHi : bgc.brighter (0.12f);
        Colour bot = on ? accent   : bgc.darker (0.18f);
        if (down) { top = top.darker (0.1f); bot = bot.darker (0.1f); }
        else if (highlighted) { top = top.brighter (0.08f); }
        ColourGradient bgrad (top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false);
        g.setGradientFill (bgrad);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.drawLine (r.getX() + 4, r.getY() + 1.4f, r.getRight() - 4, r.getY() + 1.4f, 1.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        return juce::Font (l.getFont().getHeight(), juce::Font::bold);
    }

    juce::Colour accent, accentHi, bg, panel, panelHi, line, text;
};
