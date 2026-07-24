#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace namapp {
namespace gui {

struct Theme {
    // Colors based on Thall aesthetic: Dark grey/black backgrounds with neon/cyan/red accents
    static inline const juce::Colour BackgroundDark = juce::Colour(0xff121212);
    static inline const juce::Colour BackgroundPanel = juce::Colour(0xff1e1e1e);
    static inline const juce::Colour AccentNeonRed = juce::Colour(0xffff2a2a);
    static inline const juce::Colour AccentNeonCyan = juce::Colour(0xff00e5ff);
    static inline const juce::Colour TextPrimary = juce::Colour(0xffffffff);
    static inline const juce::Colour TextSecondary = juce::Colour(0xff888888);
    static inline const juce::Colour MeterGreen = juce::Colour(0xff00ff00);
    static inline const juce::Colour MeterYellow = juce::Colour(0xffffff00);
    static inline const juce::Colour MeterRed = juce::Colour(0xffff0000);

    // Custom LookAndFeel for rotary sliders
    class ThallLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        ThallLookAndFeel() {
            setColour(juce::Slider::rotarySliderFillColourId, AccentNeonRed);
            setColour(juce::Slider::thumbColourId, TextPrimary);
            setColour(juce::Slider::trackColourId, juce::Colour(0xff333333));
        }
        
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                              const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override {
            auto radius = (float) juce::jmin (width / 2, height / 2) - 4.0f;
            auto centreX = (float) x + (float) width  * 0.5f;
            auto centreY = (float) y + (float) height * 0.5f;
            auto rx = centreX - radius;
            auto ry = centreY - radius;
            auto rw = radius * 2.0f;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            // Draw track
            g.setColour(findColour(juce::Slider::trackColourId));
            g.drawEllipse(rx, ry, rw, rw, 4.0f);

            // Draw fill
            juce::Path p;
            auto pointerLength = radius * 0.8f;
            auto pointerThickness = 4.0f;
            p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
            p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
            
            g.setColour(findColour(juce::Slider::rotarySliderFillColourId));
            g.fillPath(p);
        }
    };
};

} // namespace gui
} // namespace namapp
