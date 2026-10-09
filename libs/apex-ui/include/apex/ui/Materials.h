#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace apex::ui
{

/**
 * Materials
 * ---------
 * Everything that has to look like a physical object is shaded per pixel here
 * (light from the top-left, like a studio key light) and cached at the
 * physical resolution it is drawn at, so it stays sharp at any UI scale and
 * costs one image blit per repaint.
 */
enum class KnobStyle
{
    spunAluminium,   // amp faceplate: knurled black skirt, spun aluminium cap
    pedal,           // stompbox: matte black cap, painted white line
    chickenHead      // rotary selector: bakelite pointer knob
};

namespace materials
{
    /** Tileable detail textures. Light pixels lighten and dark pixels darken
        whatever they are drawn over, so they add grain without shifting the
        base colour. Use with Graphics::setTiledImageFill at low opacity. */
    const juce::Image& brushedMetal();
    const juce::Image& tolexGrain();
    const juce::Image& powderGrain();
    const juce::Image& grilleCloth();

    /** Static (light-fixed) knob body including its drop shadow. The image is
        1.5 x the knob diameter; the knob is centred in it. */
    juce::Image knob (KnobStyle style, int diameterPx);

    /** Chrome stomp switch (nut, washer, domed cap), 1.3 x diameter, centred. */
    juce::Image footswitch (int diameterPx, bool pressed);
}

namespace draw
{
    /** Draws a knob of the given style in `area` (square), indicator at `angle`
        radians clockwise from 12 o'clock. */
    void knob (juce::Graphics&, juce::Rectangle<float> area, float angle, KnobStyle style);

    /** Printed scale around a knob: ticks every step and numbers 0..10. */
    void knobScale (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour colour,
                    bool withNumbers, int numTicks = 11);

    void footswitch (juce::Graphics&, juce::Rectangle<float> area, bool pressed);

    /** Chrome-bezel LED with glow. `area` is the bezel; the glow extends past it. */
    void led (juce::Graphics&, juce::Rectangle<float> area, bool on, juce::Colour colour);

    void screw (juce::Graphics&, juce::Point<float> centre, float radius, float slotAngle);

    /** Faceted amp pilot jewel with glow. */
    void jewel (juce::Graphics&, juce::Rectangle<float> area, bool on, juce::Colour colour);

    /** Chrome bat-handle toggle switch seen from the front, lever up or down. */
    void batToggle (juce::Graphics&, juce::Rectangle<float> area, bool up);

    /** Text with a dark shadow below and a faint highlight above: engraved /
        silkscreened on metal. */
    void silkscreen (juce::Graphics&, const juce::String& text, const juce::Font& font,
                     juce::Rectangle<float> area, juce::Justification just, juce::Colour colour);

    /** Polished chrome lettering (badge). */
    void chromeText (juce::Graphics&, const juce::String& text, const juce::Font& font,
                     juce::Rectangle<float> area, juce::Justification just);

    /** Smoked display glass: inner shadow, black body and a diagonal sheen. */
    void displayGlass (juce::Graphics&, juce::Rectangle<float> area, float cornerRadius);

    /** Seven-segment LED digits with unlit ghost segments and a glow. Supports
        0-9, '-', '+', ' ', and A b C d E F G H L n o P r t U. */
    void sevenSegment (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                       juce::Colour lit, int numCells);

    /** Patch cable along `path` with jack plugs drawn by the caller. */
    void cable (juce::Graphics&, const juce::Path& path, float thickness);

    /** Chrome jack plug pointing in `direction` (1 = right, -1 = left) whose tip
        sits at `tip`. */
    void jackPlug (juce::Graphics&, juce::Point<float> tip, float height, int direction);

    /** Soft elliptical contact shadow under an object resting on a surface. */
    void contactShadow (juce::Graphics&, juce::Rectangle<float> objectBounds, float spread, float opacity);

    /** Fills a rounded rectangle and overlays a tiled detail texture. */
    void texturedFill (juce::Graphics&, const juce::Path& shape, juce::Colour base,
                       const juce::Image& texture, float textureOpacity, float textureScale = 1.0f);
}

} // namespace apex::ui
