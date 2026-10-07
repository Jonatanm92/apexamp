#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "apex/ui/LookAndFeel.h"
#include "apex/ui/PresetManager.h"
#include "apex/ui/Shell.h"

namespace apex::ui
{

/**
 * EditorBase
 * ----------
 * Shared frame for every Apex editor: the header bar (presets, A/B, undo/redo,
 * meters, tuner, settings), a fixed-size design canvas that is scaled as a whole
 * (crisp at any size; the size is remembered per instance), keyboard shortcuts
 * and the tuner overlay. Subclasses build their stage in `stage` using design
 * coordinates (designWidth x designHeight - header).
 */
class EditorBase : public juce::AudioProcessorEditor,
                   private juce::Timer
{
public:
    static constexpr int headerHeight = 48;

    EditorBase (juce::AudioProcessor&, juce::AudioProcessorValueTreeState&, PresetManager&,
                const juce::String& productName, int designWidth, int designHeight, TunerFeed* tuner);
    ~EditorBase() override;

    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

protected:
    /** Called 30 times a second on the message thread. */
    virtual void tick() {}
    /** Linear peak levels for the header meters. */
    virtual float getInputPeak()  { return 0.0f; }
    virtual float getOutputPeak() { return 0.0f; }
    /** Extra settings-menu entries (ids >= 100) and their handler. */
    virtual void addSettingsItems (juce::PopupMenu&) {}
    virtual void handleSettingsItem (int) {}

    juce::Component stage;   // design-space area under the header
    ApexLookAndFeel lookAndFeel;
    juce::AudioProcessorValueTreeState& apvts;
    PresetManager& presets;

private:
    void timerCallback() override;
    void showPresetMenu();
    void showSaveDialog();
    void showSettingsMenu();
    void setUiScale (float);
    void refreshHeader();

    juce::Component content;   // designWidth x designHeight, scaled by a transform
    HeaderBar header;
    std::unique_ptr<TunerOverlay> tuner;
    juce::TooltipWindow tooltips { this, 700 };
    std::unique_ptr<juce::AlertWindow> saveWindow;
    int designW, designH;
    bool constructed = false;
};

} // namespace apex::ui
