#include "apex/ui/EditorBase.h"
#include "apex/ui/Theme.h"

#include <iterator>

namespace apex::ui
{

namespace
{
    constexpr float scales[] = { 0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 2.0f };
}

EditorBase::EditorBase (juce::AudioProcessor& processor, juce::AudioProcessorValueTreeState& state,
                        PresetManager& presetManager, const juce::String& productName,
                        int designWidth, int designHeight, TunerFeed* tunerFeed)
    : juce::AudioProcessorEditor (processor), apvts (state), presets (presetManager),
      header (productName), designW (designWidth), designH (designHeight)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    addAndMakeVisible (content);
    content.addAndMakeVisible (stage);
    content.addAndMakeVisible (header);

    if (tunerFeed != nullptr)
    {
        tuner = std::make_unique<TunerOverlay> (*tunerFeed);
        content.addChildComponent (*tuner);
        tuner->onClose = [this] { header.tunerButton.setToggleState (false, juce::dontSendNotification); };
        header.tunerButton.onClick = [this]
        {
            if (tuner->isVisible()) tuner->close();
            else { tuner->open(); header.tunerButton.setToggleState (true, juce::dontSendNotification); }
        };
    }
    header.setShowsTuner (tunerFeed != nullptr);

    header.previousButton.onClick = [this] { presets.loadPrevious(); refreshHeader(); };
    header.nextButton.onClick     = [this] { presets.loadNext();     refreshHeader(); };
    header.presetBox.onClick      = [this] { showPresetMenu(); };
    header.saveButton.onClick     = [this] { showSaveDialog(); };
    header.aButton.onClick        = [this] { presets.selectSlot (0); refreshHeader(); };
    header.bButton.onClick        = [this] { presets.selectSlot (1); refreshHeader(); };
    header.undoButton.onClick     = [this] { if (auto* u = apvts.undoManager) u->undo(); };
    header.redoButton.onClick     = [this] { if (auto* u = apvts.undoManager) u->redo(); };
    header.settingsButton.onClick = [this] { showSettingsMenu(); };

    content.setBounds (0, 0, designW, designH);
    header.setBounds (0, 0, designW, headerHeight);
    stage.setBounds (0, headerHeight, designW, designH - headerHeight);
    if (tuner != nullptr)
        tuner->setBounds (stage.getBounds());

    setResizable (true, true);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) designW / (double) designH);
    setResizeLimits (juce::roundToInt (designW * 0.6f), juce::roundToInt (designH * 0.6f), designW * 2, designH * 2);

    const float saved = juce::jlimit (0.6f, 2.0f, (float) (double) apvts.state.getProperty ("uiScale", 1.0));
    setSize (juce::roundToInt ((float) designW * saved), juce::roundToInt ((float) designH * saved));
    constructed = true;

    setWantsKeyboardFocus (true);
    refreshHeader();
    startTimerHz (30);
}

EditorBase::~EditorBase()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EditorBase::resized()
{
    if (getWidth() <= 0)
        return;
    const float scale = (float) getWidth() / (float) designW;
    content.setTransform (juce::AffineTransform::scale (scale));
    if (constructed)
        apvts.state.setProperty ("uiScale", scale, nullptr);
}

void EditorBase::setUiScale (float s)
{
    setSize (juce::roundToInt ((float) designW * s), juce::roundToInt ((float) designH * s));
}

bool EditorBase::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() && key.getKeyCode() == 'Z')
    {
        if (auto* u = apvts.undoManager)
            mods.isShiftDown() ? u->redo() : u->undo();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'Y')
    {
        if (auto* u = apvts.undoManager)
            u->redo();
        return true;
    }
    if (key == juce::KeyPress::escapeKey && tuner != nullptr && tuner->isVisible())
    {
        tuner->close();
        return true;
    }
    return false;
}

void EditorBase::refreshHeader()
{
    header.presetBox.setPreset (presets.getCurrentName(), presets.isModified());
    header.aButton.setToggleState (presets.getSlot() == 0, juce::dontSendNotification);
    header.bButton.setToggleState (presets.getSlot() == 1, juce::dontSendNotification);
    if (auto* u = apvts.undoManager)
    {
        header.undoButton.setEnabled (u->canUndo());
        header.redoButton.setEnabled (u->canRedo());
    }
}

void EditorBase::timerCallback()
{
    header.inMeter.pushPeak (getInputPeak());
    header.outMeter.pushPeak (getOutputPeak());
    refreshHeader();
    tick();
}

void EditorBase::showPresetMenu()
{
    presets.refreshUserPresets();
    juce::PopupMenu menu;
    menu.addSectionHeader ("Factory");
    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
        menu.addItem (i + 1, presets.getPresetName (i), true, i == presets.getCurrentIndex());

    if (presets.getNumPresets() > presets.getNumFactoryPresets())
    {
        menu.addSectionHeader ("User");
        for (int i = presets.getNumFactoryPresets(); i < presets.getNumPresets(); ++i)
            menu.addItem (i + 1, presets.getPresetName (i), true, i == presets.getCurrentIndex());
    }
    menu.addSeparator();
    menu.addItem (10001, "Save As" + juce::String::charToString (0x2026));
    menu.addItem (10002, "Open Preset Folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&header.presetBox)
                                                  .withMinimumWidth (header.presetBox.getWidth()),
                        [this] (int result)
                        {
                            if (result == 10001)      showSaveDialog();
                            else if (result == 10002) { presets.getUserFolder().createDirectory(); presets.getUserFolder().startAsProcess(); }
                            else if (result > 0)      presets.loadPreset (result - 1);
                            refreshHeader();
                        });
}

void EditorBase::showSaveDialog()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save preset", "Name this sound.", juce::MessageBoxIconType::NoIcon, this);
    saveWindow->setLookAndFeel (&lookAndFeel);
    saveWindow->addTextEditor ("name", presets.getCurrentName(), {});
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveWindow != nullptr)
            presets.saveUserPreset (saveWindow->getTextEditorContents ("name"));
        saveWindow.reset();
        refreshHeader();
    }), false);
}

void EditorBase::showSettingsMenu()
{
    juce::PopupMenu menu;
    juce::PopupMenu size;
    const float current = (float) getWidth() / (float) designW;
    for (int i = 0; i < (int) std::size (scales); ++i)
        size.addItem (1 + i, juce::String (juce::roundToInt (scales[i] * 100.0f)) + " %", true,
                      std::abs (current - scales[i]) < 0.02f);
    menu.addSubMenu ("Window size", size);
    addSettingsItems (menu);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&header.settingsButton),
                        [this] (int result)
                        {
                            if (result >= 1 && result <= (int) std::size (scales))
                                setUiScale (scales[result - 1]);
                            else if (result >= 100)
                                handleSettingsItem (result);
                        });
}

} // namespace apex::ui
