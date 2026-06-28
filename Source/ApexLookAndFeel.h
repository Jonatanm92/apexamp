#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    Dark, modern look for ApexAmp: charcoal panels with an amber accent, custom
    rotary knobs (ring + value arc + pointer), and styled combo boxes / buttons.
*/
class ApexLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ApexLookAndFeel()
    {
        using juce::Colour;
        accent  = Colour (0xffff7a18);   // amber/orange
        panel   = Colour (0xff23262d);
        panelHi = Colour (0xff2e323b);
        line    = Colour (0xff121317);

        setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::textColourId, juce::Colours::white.withAlpha (0.9f));
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::arrowColourId, accent);

        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.35f));
        setColour (juce::PopupMenu::textColourId, juce::Colours::white.withAlpha (0.9f));

        setColour (juce::TextButton::buttonColourId, panelHi);
        setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.9f));
        setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.9f));
        setColour (juce::ToggleButton::tickColourId, accent);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        using namespace juce;
        auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
        const auto radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle  = startAngle + sliderPos * (endAngle - startAngle);

        // body
        g.setColour (panelHi);
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (line);
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);

        // track
        const float arcR = radius - 3.0f;
        Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

        // value arc
        Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (accent);
        g.strokePath (value, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

        // pointer
        Path ptr;
        const float ptrLen = radius * 0.62f;
        ptr.addRoundedRectangle (-1.6f, -ptrLen, 3.2f, ptrLen, 1.5f);
        ptr.applyTransform (AffineTransform::rotation (angle).translated (centre.x, centre.y));
        g.setColour (Colours::white.withAlpha (0.92f));
        g.fillPath (ptr);
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        return juce::Font (l.getFont().getHeight(), juce::Font::bold);
    }

    juce::Colour accent, panel, panelHi, line;
};
