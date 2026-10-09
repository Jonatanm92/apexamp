#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <map>
#include <vector>

namespace apex::ui
{

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;   // parameter id -> real value
};

/**
 * PresetManager
 * -------------
 * Factory + user presets, the "modified" indicator and A/B compare for an
 * APVTS. Lives in the processor so it survives the editor being closed.
 *
 * Loading a preset first returns every preset-scoped parameter to its default,
 * then applies the preset's values, so presets are complete snapshots even when
 * new parameters are added later. User presets are XML files in
 *   <user app data>/Apex/<product>/Presets/*.apexpreset
 * All methods are for the message thread.
 */
class PresetManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    PresetManager (juce::AudioProcessorValueTreeState&, juce::String productName,
                   std::vector<FactoryPreset> factory, juce::StringArray nonPresetParameterIds);
    ~PresetManager() override;

    int getNumPresets() const;
    juce::String getPresetName (int index) const;
    bool isFactoryPreset (int index) const { return index < (int) factory.size(); }
    int getNumFactoryPresets() const { return (int) factory.size(); }

    void loadPreset (int index);
    void loadNext()     { step (1); }
    void loadPrevious() { step (-1); }

    int getCurrentIndex() const noexcept { return currentIndex; }
    juce::String getCurrentName() const;
    bool isModified() const noexcept { return modified.load(); }

    /** Saves the current settings as a user preset (overwrites same name). */
    bool saveUserPreset (const juce::String& name);
    juce::File getUserFolder() const;
    void refreshUserPresets();

    /** A/B compare: 0 = A, 1 = B. Switching stores the current settings in the
        slot being left and recalls the other one (a copy the first time). */
    void selectSlot (int slot);
    int getSlot() const noexcept { return slot; }

    /** Persist the preset name / modified flag inside the plugin state. */
    void writeTo (juce::ValueTree& state) const;
    void readFrom (const juce::ValueTree& state);

private:
    using Snapshot = std::map<juce::String, float>;   // id -> normalised value

    void parameterChanged (const juce::String&, float) override;
    void step (int delta);
    void applyValues (const std::vector<std::pair<juce::String, float>>& realValues);
    void applySnapshot (const Snapshot&);
    Snapshot capture() const;
    bool isPresetParameter (const juce::String& id) const { return ! excluded.contains (id); }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String product;
    std::vector<FactoryPreset> factory;
    juce::StringArray excluded;
    juce::Array<juce::File> userFiles;

    int currentIndex = 0;
    juce::String currentName;
    std::atomic<bool> modified { false };
    std::atomic<bool> applying { false };   // automation may arrive on the audio thread

    int slot = 0;
    Snapshot slots[2];
    bool slotValid[2] { false, false };
};

} // namespace apex::ui
