#include "apex/ui/PresetManager.h"

namespace apex::ui
{

namespace
{
    const juce::String presetExtension (".apexpreset");
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state, juce::String productName,
                              std::vector<FactoryPreset> factoryPresets, juce::StringArray nonPresetParameterIds)
    : apvts (state), product (std::move (productName)), factory (std::move (factoryPresets)),
      excluded (std::move (nonPresetParameterIds))
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (rp->getParameterID(), this);

    currentName = factory.empty() ? juce::String ("Default") : factory.front().name;
    refreshUserPresets();
}

PresetManager::~PresetManager()
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (rp->getParameterID(), this);
}

void PresetManager::parameterChanged (const juce::String& id, float)
{
    if (! applying && isPresetParameter (id))
        modified.store (true);
}

int PresetManager::getNumPresets() const
{
    return (int) factory.size() + userFiles.size();
}

juce::String PresetManager::getPresetName (int index) const
{
    if (index < (int) factory.size())
        return factory[(size_t) index].name;
    if (juce::isPositiveAndBelow (index - (int) factory.size(), userFiles.size()))
        return userFiles[index - (int) factory.size()].getFileNameWithoutExtension();
    return {};
}

juce::String PresetManager::getCurrentName() const
{
    return currentName;
}

juce::File PresetManager::getUserFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Apex").getChildFile (product).getChildFile ("Presets");
}

void PresetManager::refreshUserPresets()
{
    userFiles = getUserFolder().findChildFiles (juce::File::findFiles, false, "*" + presetExtension);
    userFiles.sort();
}

void PresetManager::applyValues (const std::vector<std::pair<juce::String, float>>& realValues)
{
    std::map<juce::String, float> wanted (realValues.begin(), realValues.end());
    const juce::ScopedValueSetter<bool> guard (applying, true);

    for (auto* p : apvts.processor.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr || ! isPresetParameter (rp->getParameterID()))
            continue;

        const auto it = wanted.find (rp->getParameterID());
        const float norm = it != wanted.end() ? rp->convertTo0to1 (it->second) : rp->getDefaultValue();
        if (std::abs (rp->getValue() - norm) > 1.0e-6f)
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (norm);
            rp->endChangeGesture();
        }
    }
}

void PresetManager::applySnapshot (const Snapshot& snapshot)
{
    const juce::ScopedValueSetter<bool> guard (applying, true);
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto it = snapshot.find (rp->getParameterID());
            if (it != snapshot.end() && std::abs (rp->getValue() - it->second) > 1.0e-6f)
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (it->second);
                rp->endChangeGesture();
            }
        }
}

PresetManager::Snapshot PresetManager::capture() const
{
    Snapshot s;
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (isPresetParameter (rp->getParameterID()))
                s[rp->getParameterID()] = rp->getValue();
    return s;
}

void PresetManager::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPresets()))
        return;

    if (auto* undo = apvts.undoManager)
        undo->beginNewTransaction ("Load preset");

    if (index < (int) factory.size())
    {
        applyValues (factory[(size_t) index].values);
    }
    else
    {
        const auto file = userFiles[index - (int) factory.size()];
        std::vector<std::pair<juce::String, float>> values;
        if (auto xml = juce::XmlDocument::parse (file))
            for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
                values.emplace_back (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));
        applyValues (values);
    }

    currentIndex = index;
    currentName = getPresetName (index);
    modified.store (false);
}

void PresetManager::step (int delta)
{
    const int n = getNumPresets();
    if (n > 0)
        loadPreset (((currentIndex + delta) % n + n) % n);
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    juce::XmlElement xml ("APEXPRESET");
    xml.setAttribute ("product", product);
    xml.setAttribute ("name", name);
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (isPresetParameter (rp->getParameterID()))
            {
                auto* e = xml.createNewChildElement ("PARAM");
                e->setAttribute ("id", rp->getParameterID());
                e->setAttribute ("value", rp->convertFrom0to1 (rp->getValue()));
            }

    const auto folder = getUserFolder();
    if (! folder.createDirectory())
        return false;
    const auto file = folder.getChildFile (name + presetExtension);
    if (! xml.writeTo (file))
        return false;

    refreshUserPresets();
    currentIndex = (int) factory.size() + userFiles.indexOf (file);
    currentName = name;
    modified.store (false);
    return true;
}

void PresetManager::selectSlot (int newSlot)
{
    newSlot = juce::jlimit (0, 1, newSlot);
    if (newSlot == slot)
        return;

    slots[slot] = capture();
    slotValid[slot] = true;
    slot = newSlot;

    if (slotValid[slot])
    {
        if (auto* undo = apvts.undoManager)
            undo->beginNewTransaction ("Compare");
        applySnapshot (slots[slot]);
    }
    else
    {
        slots[slot] = slots[1 - slot];
        slotValid[slot] = true;
    }
}

void PresetManager::writeTo (juce::ValueTree& state) const
{
    state.setProperty ("presetName", currentName, nullptr);
    state.setProperty ("presetIndex", currentIndex, nullptr);
    state.setProperty ("presetModified", modified.load(), nullptr);
}

void PresetManager::readFrom (const juce::ValueTree& state)
{
    if (state.hasProperty ("presetName"))
    {
        currentName  = state.getProperty ("presetName").toString();
        currentIndex = (int) state.getProperty ("presetIndex", 0);
        modified.store ((bool) state.getProperty ("presetModified", false));
    }
}

} // namespace apex::ui
