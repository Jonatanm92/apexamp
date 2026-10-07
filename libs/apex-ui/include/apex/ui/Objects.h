#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace apex::ui::draw
{

/** Dark studio backdrop: a soft key light above `focus` falling off into black. */
void stage (juce::Graphics&, juce::Rectangle<float> bounds, juce::Point<float> focus);

/** Pedalboard deck (grip-taped plank with aluminium rails). */
void pedalboard (juce::Graphics&, juce::Rectangle<float> bounds);

/** Geometry only: the flat top face of pedalEnclosure (bounds). */
juce::Rectangle<float> pedalTopFace (juce::Rectangle<float> bounds);

/** Geometry only: the faceplate area of ampHead (bounds). */
juce::Rectangle<float> ampHeadFaceplate (juce::Rectangle<float> bounds);

/** Chrome top-mounted jack plug whose tip enters a pedal at `tip`; the boot
    points up. Returns where the cable leaves the boot. */
juce::Point<float> topJackPlug (juce::Graphics&, juce::Point<float> tip, float width);

/** Die-cast stompbox enclosure seen from above: bevelled edges, metallic
    paint, sheen. Returns the flat top face (where controls go). */
juce::Rectangle<float> pedalEnclosure (juce::Graphics&, juce::Rectangle<float> bounds, juce::Colour paint);

/** Amp head: tolex box, cream piping, corner protectors and leather handle.
    Returns the faceplate area (drawn by drawFaceplate). */
juce::Rectangle<float> ampHead (juce::Graphics&, juce::Rectangle<float> bounds);

/** 4x12 cabinet front: tolex frame, piping and woven grille cloth. Returns the
    grille area. */
juce::Rectangle<float> speakerCab (juce::Graphics&, juce::Rectangle<float> bounds);

/** Black brushed-aluminium faceplate with a chamfered edge and four screws. */
void faceplate (juce::Graphics&, juce::Rectangle<float> area);

/** Thin engraved divider line with an optional centred caption. */
void engravedRule (juce::Graphics&, juce::Rectangle<float> area, const juce::String& caption, juce::Colour colour);

} // namespace apex::ui::draw
