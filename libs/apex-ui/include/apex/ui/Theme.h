#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace apex::ui
{

/** The Apex palette. Amber means one thing: "on". */
namespace colours
{
    inline const juce::Colour stageTop     { 0xff17171a };
    inline const juce::Colour stageBottom  { 0xff060607 };
    inline const juce::Colour headerBg     { 0xff0b0b0c };
    inline const juce::Colour hairline     { 0xff232327 };
    inline const juce::Colour panel        { 0xff141416 };
    inline const juce::Colour recess       { 0xff08080a };

    inline const juce::Colour bone         { 0xffece7dc };   // primary text / silkscreen
    inline const juce::Colour label        { 0xffbcb6aa };
    inline const juce::Colour dim          { 0xff8e897f };
    inline const juce::Colour faint        { 0xff5d5a55 };

    inline const juce::Colour amber        { 0xffff8a1f };   // "on"
    inline const juce::Colour amberDeep    { 0xffb85200 };
    inline const juce::Colour ledOff       { 0xff3a1d0c };
    inline const juce::Colour danger       { 0xffff4a3d };   // clip only

    inline const juce::Colour tolex        { 0xff151516 };
    inline const juce::Colour piping       { 0xffcfc8b8 };
    inline const juce::Colour faceplate    { 0xff1a1a1c };
}

/** Embedded OFL fonts (Barlow Condensed, Big Shoulders Display, JetBrains Mono). */
struct Fonts
{
    enum class Face { labelMedium, labelSemiBold, labelBold, display, monoMedium, monoBold };

    static juce::Font get (Face face, float height, float kerning = 0.0f);

    static juce::Font label (float h, float kerning = 0.08f)   { return get (Face::labelSemiBold, h, kerning); }
    static juce::Font labelBold (float h, float kerning = 0.1f) { return get (Face::labelBold, h, kerning); }
    static juce::Font body (float h)                           { return get (Face::labelMedium, h, 0.02f); }
    static juce::Font display (float h, float kerning = 0.12f) { return get (Face::display, h, kerning); }
    static juce::Font mono (float h)                           { return get (Face::monoMedium, h); }
    static juce::Font monoBold (float h)                       { return get (Face::monoBold, h); }
};

} // namespace apex::ui
