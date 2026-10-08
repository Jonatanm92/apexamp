#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "apex/ui/LookAndFeel.h"
#include "apex/ui/Shell.h"

#include <functional>
#include <vector>

/**
 * apex::ui::abyss
 * ---------------
 * The "abyss" visual language: black basalt with ember-lit cracks, thorned
 * gothic frames, glowing ember indicators and Cinzel lettering. Everything is
 * rendered in code (procedural textures, vector paths, glow passes), so every
 * control is a live component, crisp at any window size.
 *
 * Static art (stone, frames, titles) is rendered once into a cached image by
 * Backdrop; components draw only what moves.
 */
namespace apex::ui::abyss
{

namespace colours
{
    inline const juce::Colour coal      { 0xff090706 };
    inline const juce::Colour stone     { 0xff17120f };
    inline const juce::Colour panel     { 0xff0d0a09 };
    inline const juce::Colour rim       { 0xff3a322d };
    inline const juce::Colour rimLight  { 0xff6d6158 };
    inline const juce::Colour ash       { 0xff8c8279 };
    inline const juce::Colour bone      { 0xffd8cdbf };
    inline const juce::Colour ember     { 0xffff6420 };
    inline const juce::Colour emberHot  { 0xffffb46a };
    inline const juce::Colour emberDeep { 0xffa8230a };
    inline const juce::Colour emberDim  { 0xff4a1a0b };
}

namespace fonts
{
    juce::Font title (float height);                              // Cinzel Decorative Black
    juce::Font serif (float height, float kerning = 0.3f);        // Cinzel Bold, spaced
    juce::Font serifLight (float height, float kerning = 0.3f);   // Cinzel SemiBold
    juce::Font label (float height, float kerning = 0.06f);       // Barlow Condensed SemiBold
    juce::Font labelBold (float height, float kerning = 0.08f);   // Barlow Condensed Bold
    juce::Font value (float height);                              // Barlow Condensed Medium
}

//==============================================================================
// Painting helpers

/** Ember colour for an intensity 0..1+ (deep red -> orange -> white-hot). */
juce::Colour emberRamp (float intensity);

/** Glow by layering: wide faint strokes under a bright core. */
void glowStroke (juce::Graphics&, const juce::Path&, float width, juce::Colour core, float intensity,
                 juce::PathStrokeType::JointStyle = juce::PathStrokeType::curved);
void glowText (juce::Graphics&, const juce::String&, const juce::Font&, juce::Rectangle<float>,
               juce::Justification, juce::Colour core, float intensity);
/** Soft round glow (radial gradient), e.g. behind LEDs and pointer tips. */
void glowSpot (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour, float alpha);

enum class Icon
{
    drop, gate, boost, amp, shape, cab, fx, eq,
    link, spiral, sun, trident, check, warning, target,
    previous, next, save, undo, redo, tuner, settings, load, echo, abyss
};
/** Icon as a stroke path fitted into the area (stroke it ~area/14 wide). */
juce::Path iconPath (Icon, juce::Rectangle<float> area);
void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float> area, juce::Colour, float glow);

/** A procedural "rune": 2-4 strokes chosen from the seed, fitted in the area. */
juce::Path runePath (int seed, juce::Rectangle<float> area);

/** Thin occult diagram: rings, a star and runes. */
void sigil (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour, float alpha, int seed);

/** Gothic frame along an outline: stone rim, braided thorned vines, ember
    sections (drawn into `glow`, which Backdrop blurs and adds). heat 0..1. */
void ornateFrame (juce::Graphics& art, juce::Graphics& glow, const juce::Path& outline, int seed, float heat,
                  float weight = 1.0f);

/** A pointed-arch panel outline (rounded bottom, ogee top with a peak). */
juce::Path archPanel (juce::Rectangle<float> bounds, float archHeight, float cornerRadius);
/** A lens / elongated hexagon (preset plate). */
juce::Path lensPlate (juce::Rectangle<float> bounds);

//==============================================================================
/**
 * Stone + ember-crack background with ornaments composited over it. Recess
 * paths are darkened and cooled (panel interiors); hotspots raise the ember
 * level locally. The ornament callback paints in design coordinates into an
 * art layer and a glow layer.
 */
struct BackdropSpec
{
    int designWidth = 0, designHeight = 0;
    std::vector<juce::Path> recesses;
    std::vector<std::pair<juce::Point<float>, float>> hotspots;   // centre, radius
    std::function<void (juce::Graphics& art, juce::Graphics& glow)> ornaments;
    int seed = 1;
};

class Backdrop : public juce::Component,
                 private juce::Timer
{
public:
    explicit Backdrop (BackdropSpec);
    void paint (juce::Graphics&) override;

    static juce::Image render (const BackdropSpec&, float pixelsPerUnit);

private:
    void timerCallback() override;

    BackdropSpec spec;
    juce::Image cache;
    float cacheScale = 0.0f, pendingScale = 0.0f;
};

namespace detail
{
    /** Image cache shared by every abyss editor; freed with JUCE's shutdown objects. */
    juce::Image& cachedImage (const juce::String& key);
}

//==============================================================================
// Controls

/** Rotary control: dark ridged knob, ember pointer and value arc. With a
    caption, the caption sits above the knob and turns into the value while the
    knob is hovered or dragged. */
class Knob : public juce::Slider
{
public:
    Knob();

    void setCaption (const juce::String& text) { caption = text; cacheKey = {}; repaint(); }
    static constexpr float captionHeight = 22.0f;

    /** Ticks light from this value to the current one (default: the minimum). */
    void setLitOrigin (double value) { litOrigin = value; hasOrigin = true; repaint(); }
    void setShowsNumbers (bool shouldShow) { numbers = shouldShow; cacheKey = {}; repaint(); }
    void setPopupParent (juce::Component* parent);

    /** The component is boundsRatio x the knob, leaving room for the scale. */
    static constexpr float boundsRatio = 1.62f;
    juce::Rectangle<float> getKnobArea() const;
    bool hitTest (int x, int y) override;
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent& e) override { juce::Slider::mouseEnter (e); repaint(); }
    void mouseExit (const juce::MouseEvent& e) override  { juce::Slider::mouseExit (e); repaint(); }

private:
    juce::String caption;
    double litOrigin = 0.0;
    bool hasOrigin = false, numbers = true;
    juce::Image cache;
    juce::String cacheKey;
};

/** Rectangular button with an optional icon; lit ember when on. */
class GlowButton : public juce::Button
{
public:
    enum class Style { tab, plain, icon, text };
    GlowButton (const juce::String& text, Style = Style::tab);
    GlowButton (Icon, const juce::String& text = {}, Style = Style::icon);

    void setIcon (Icon i) { icon = i; hasIcon = true; repaint(); }
    void setPulsing (bool shouldPulse) { pulsing = shouldPulse; }
    void setPulse (float amount) { pulse = amount; repaint(); }
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Style style;
    Icon icon = Icon::check;
    bool hasIcon = false, pulsing = false;
    float pulse = 0.0f;
};

/** One module of the signal chain: name, icon, an enable bar. Clicking the
    block selects it; clicking the bar toggles the module. */
class ChainBlock : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    ChainBlock (const juce::String& name, Icon);

    void setSelected (bool);
    void setEnabledState (bool);
    void setActivity (float level);   // 0..1, warms the icon while signal passes
    /** The module's key setting, shown on the block (e.g. "-5 ST"). */
    void setValueText (const juce::String&);
    std::function<void()> onSelect, onToggle;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    juce::Rectangle<float> barArea() const;
    juce::String name, valueText;
    Icon icon;
    bool selected = false, enabledState = true;
    float activity = 0.0f;
};

/** Crackling ember link between chain blocks; animated by the signal level. */
class ChainLink : public juce::Component
{
public:
    void setLevel (float level01);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f;
    int frame = 0;
};

/** Input / output ring. Optionally clickable (e.g. the output ring is power). */
class Jack : public juce::Button
{
public:
    explicit Jack (const juce::String& name);
    void setLevel (float level01) { if (std::abs (level - level01) > 0.01f) { level = level01; repaint(); } }
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    float level = 0.0f;
};

/** Vertical segmented meter with optional scale and readout. Levels in dB. */
class VMeter : public juce::Component
{
public:
    VMeter (float minDb, float maxDb, std::vector<float> scaleMarks, bool showReadout);
    void pushLevel (float db);
    void setTargetMark (bool show) { targetMark = show; }
    void paint (juce::Graphics&) override;

private:
    float minDb, maxDb;
    std::vector<float> marks;
    bool readout, targetMark = false;
    float level = -100.0f, hold = -100.0f, shown = -100.0f;
    int holdFrames = 0;
};

/** Calibration radar: blips at distance |level - target| from the centre. */
class Radar : public juce::Component,
              public juce::SettableTooltipClient
{
public:
    void push (float levelDb, float targetDb, bool playing);
    void paint (juce::Graphics&) override;

private:
    struct Blip { float angle, radius, life; };
    std::vector<Blip> blips;
    float sweep = 0.0f, centreGlow = 0.0f;
};

/** Oscilloscope trace. */
class Scope : public juce::Component
{
public:
    void setSamples (const std::vector<float>& s) { samples = s; repaint(); }
    void paint (juce::Graphics&) override;

private:
    std::vector<float> samples;
};

/** Round gauge with a needle, 0..1. */
class Gauge : public juce::Component
{
public:
    void setValue (float v);
    void paint (juce::Graphics&) override;

private:
    float value = 0.0f;
};

/** Thin segmented horizontal bar, 0..1. */
class HBar : public juce::Component
{
public:
    void setValue (float v);
    void paint (juce::Graphics&) override;

private:
    float value = 0.0f;
};

/** The ember portal: a ring of cracked, glowing rock around a dark centre. */
class Portal : public juce::Component
{
public:
    enum class Style { chamber, ring };
    explicit Portal (Style s = Style::chamber) : style (s) { setInterceptsMouseClicks (false, false); }
    void setIntensity (float v);
    void paint (juce::Graphics&) override;

private:
    Style style;
    float intensity = 0.0f, phase = 0.0f;
    juce::Image cache;
    float cacheScale = 0.0f;
};

/** Horizontal slider: a dark groove with an ember fill and a glowing bead. */
class LinearSlider : public juce::Slider
{
public:
    LinearSlider();
    void paint (juce::Graphics&) override;
    void setPopupParent (juce::Component* parent);
};

/** Menus, tooltips, value bubbles, dialogs and text buttons in the abyss style. */
class LookAndFeel : public ApexLookAndFeel
{
public:
    LookAndFeel();

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    juce::Font getPopupMenuFont() override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip,
                     const juce::Rectangle<float>& body) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOverButton, bool isButtonDown) override;

    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
};

/** The strobe tuner in ember and Cinzel. */
TunerTheme tunerTheme();

/** Binds a knob or slider to a parameter (value bubble text, double-click to
    default, one undo step per gesture). */
std::unique_ptr<juce::SliderParameterAttachment> attach (juce::Slider&, juce::RangedAudioParameter&, juce::UndoManager*);

} // namespace apex::ui::abyss
