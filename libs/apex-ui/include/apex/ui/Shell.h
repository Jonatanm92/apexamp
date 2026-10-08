#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "apex/ui/Controls.h"
#include "apex/ui/TunerFeed.h"

#include <functional>
#include <vector>

namespace apex::ui
{

//==============================================================================
/** Flat icon button for the header bar. */
class IconButton : public juce::Button
{
public:
    enum class Icon { previous, next, save, undo, redo, tuner, settings, close };

    IconButton (Icon, const juce::String& tooltip);
    void paintButton (juce::Graphics&, bool over, bool down) override;

    static juce::Path pathFor (Icon);

private:
    Icon icon;
};

/** Preset name box: name, modified dot and a menu chevron. */
class PresetBox : public juce::Button
{
public:
    PresetBox();
    void setPreset (const juce::String& name, bool modified);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::String name;
    bool modified = false;
};

//==============================================================================
/** The bar across the top of every Apex plugin. */
class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar (const juce::String& productName);

    void paint (juce::Graphics&) override;
    void resized() override;

    IconButton previousButton { IconButton::Icon::previous, "Previous preset" };
    PresetBox  presetBox;
    IconButton nextButton     { IconButton::Icon::next, "Next preset" };
    IconButton saveButton     { IconButton::Icon::save, "Save preset" };
    juce::TextButton aButton  { "A" }, bButton { "B" };
    IconButton undoButton     { IconButton::Icon::undo, "Undo" };
    IconButton redoButton     { IconButton::Icon::redo, "Redo" };
    LevelMeter inMeter, outMeter;
    juce::TextButton autoButton { "AUTO" };
    IconButton tunerButton    { IconButton::Icon::tuner, "Tuner" };
    IconButton settingsButton { IconButton::Icon::settings, "Settings" };

    /** Hide the tuner button for plugins without one. */
    void setShowsTuner (bool shouldShow);

    /** Show the AUTO input-level button next to the meters. */
    void setShowsAutoInput (bool shouldShow);

private:
    juce::String product;
    juce::Rectangle<int> meterArea;
    bool showsTuner = true, showsAutoInput = false;
};

//==============================================================================
/** Colours and type of the tuner, so each visual language can restyle it. */
struct TunerTheme
{
    juce::Colour accent, inTune, text, dim, faint, panelTop, panelBottom, outline;
    std::function<juce::Font (float)> noteFont, labelFont, readoutFont;

    static TunerTheme apex();
};

/** Full-window strobe tuner. Opening it starts the feed; closing stops it. */
class TunerOverlay : public juce::Component, private juce::Timer
{
public:
    explicit TunerOverlay (TunerFeed&);
    ~TunerOverlay() override;

    void setTheme (TunerTheme t) { theme = std::move (t); repaint(); }

    void open();
    void close();
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    float detectPitch();

    TunerFeed& feed;
    std::vector<float> history, scratch, diff;
    int historyFill = 0;
    float frequency = 0.0f, smoothedCents = 0.0f, strobePhase = 0.0f;
    int note = -1, silentFrames = 0;
    bool muteOutput = true;

    juce::TextButton muteButton { "MUTE OUTPUT" };
    IconButton closeButton { IconButton::Icon::close, "Close tuner" };
    TunerTheme theme = TunerTheme::apex();
};

} // namespace apex::ui
