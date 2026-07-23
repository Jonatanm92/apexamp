#include "PluginEditor.h"

ApexAmpEditor::ApexAmpEditor (ApexAmpProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    auto& apvts = proc.apvts;

    // --- preset bar ---
    {
        const auto presets = apexpresets::factory();
        for (int i = 0; i < (int) presets.size(); ++i)
            presetBox.addItem (presets[i].name, i + 1);
        presetBox.setTextWhenNothingSelected ("Presets");
        addAndMakeVisible (presetBox);
        presetBox.onChange = [this]
        {
            const int id = presetBox.getSelectedId();
            const auto list = apexpresets::factory();
            if (id >= 1 && id <= (int) list.size())
                apexpresets::apply (proc.apvts, list[(size_t) (id - 1)]);
        };
    }

    addAndMakeVisible (savePresetButton);
    savePresetButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Save preset", juce::File{}, "*.apreset");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f != juce::File{})
                {
                    if (f.getFileExtension().isEmpty())
                        f = f.withFileExtension ("apreset");
                    proc.savePresetToFile (f);
                }
            });
    };

    addAndMakeVisible (loadPresetButton);
    loadPresetButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Load preset", juce::File{}, "*.apreset");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    proc.loadPresetFromFile (f);
                    updateIRLabel();
                }
            });
    };

    // --- selectors ---
    channelBox.addItemList ({ "Tight", "Scoop" }, 1);
    addAndMakeVisible (channelBox);
    channelAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, "channel", channelBox);

    tonestackBox.addItemList ({ "Marshall", "Fender", "Mesa", "Modern Metal" }, 1);
    addAndMakeVisible (tonestackBox);
    tonestackAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, "tonestack", tonestackBox);

    cabTypeBox.addItemList ({ "Modern V30", "Vintage Greenback", "Tight 4x12", "American Scooped" }, 1);
    addAndMakeVisible (cabTypeBox);
    cabTypeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, "cabType", cabTypeBox);

    addAndMakeVisible (cabButton);
    cabAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "cabOn", cabButton);

    addAndMakeVisible (loadIRButton);
    loadIRButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Select a cabinet impulse response", juce::File{}, "*.wav;*.aiff;*.aif");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    proc.loadCabIR (file);
                    updateIRLabel();
                }
            });
    };

    addAndMakeVisible (clearIRButton);
    clearIRButton.onClick = [this]
    {
        proc.clearCabIR();
        updateIRLabel();
    };

    irLabel.setJustificationType (juce::Justification::centredLeft);
    irLabel.setFont (juce::Font (12.0f));
    irLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (irLabel);

    auto mk = [&apvts] (std::unique_ptr<Knob>& k, const char* id, const char* cap)
    {
        k = std::make_unique<Knob> (apvts, id, cap);
    };

    mk (kInput,    "inputTrim",    "Input");
    mk (kGain,     "gain",         "Gain");
    mk (kPush,     "push",         "Push");
    mk (kTight,    "tight",        "Tight");
    mk (kSuperCut, "superCut",     "Super Cut");
    mk (kBias,     "bias",         "Bias");
    mk (kBass,     "bass",         "Bass");
    mk (kMid,      "mid",          "Mid");
    mk (kTreble,   "treble",       "Treble");
    mk (kChug,     "chug",         "Chug");
    mk (kLowDrv,   "lowDirtDrive", "Low Drv");
    mk (kLowMix,   "lowDirtMix",   "Low Mix");
    mk (kSag,      "sag",          "Sag");
    mk (kPower,    "powerDrive",   "Power");
    mk (kGate,     "gate",         "Gate");
    mk (kMaster,   "master",       "Master");

    for (auto* k : { kInput.get(), kGain.get(), kPush.get(), kTight.get(), kSuperCut.get(),
                     kBias.get(), kBass.get(), kMid.get(), kTreble.get(), kChug.get(),
                     kLowDrv.get(), kLowMix.get(), kSag.get(), kPower.get(), kGate.get(),
                     kMaster.get() })
        addAndMakeVisible (k);

    updateIRLabel();
    setSize (760, 420);
}

void ApexAmpEditor::updateIRLabel()
{
    const auto f = proc.getCabIRFile();
    irLabel.setText (f.existsAsFile() ? ("IR: " + f.getFileName()) : "Built-in cab",
                     juce::dontSendNotification);
}

void ApexAmpEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient grad (juce::Colour (0xff1b1d22), 0, 0,
                               juce::Colour (0xff0c0d10), 0, (float) getHeight(), false);
    g.setGradientFill (grad);
    g.fillAll();

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.drawText ("APEX AMP", 16, 10, 300, 32, juce::Justification::left);

    g.setColour (juce::Colours::orange.withAlpha (0.8f));
    g.setFont (juce::Font (12.0f));
    g.drawText ("beta", 150, 20, 60, 18, juce::Justification::left);

    // section dividers
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine (110, 12.0f, (float) getWidth() - 12.0f);
    g.drawHorizontalLine (250, 12.0f, (float) getWidth() - 12.0f);

    auto sectionLabel = [&g] (const juce::String& t, int x, int y)
    {
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (t, x, y, 160, 14, juce::Justification::left);
    };
    sectionLabel ("PREAMP",    16, 116);
    sectionLabel ("TONE",      300, 116);
    sectionLabel ("DYNAMICS",  500, 116);
    sectionLabel ("POWER / OUTPUT", 16, 256);
}

void ApexAmpEditor::resized()
{
    // Preset bar lives in the title band (top-right), so the knob layout below
    // is unaffected.
    presetBox.setBounds        (300, 12, 246, 26);
    savePresetButton.setBounds (552, 12, 88, 26);
    loadPresetButton.setBounds (646, 12, 88, 26);

    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (40); // title

    // top selector row
    auto top = area.removeFromTop (60);
    auto row = top.withSizeKeepingCentre (top.getWidth(), 26);
    channelBox.setBounds   (row.removeFromLeft (104)); row.removeFromLeft (6);
    tonestackBox.setBounds (row.removeFromLeft (124)); row.removeFromLeft (6);
    cabTypeBox.setBounds   (row.removeFromLeft (148)); row.removeFromLeft (6);
    cabButton.setBounds    (row.removeFromLeft (48));   row.removeFromLeft (4);
    loadIRButton.setBounds (row.removeFromLeft (78));   row.removeFromLeft (4);
    clearIRButton.setBounds (row.removeFromLeft (68));  row.removeFromLeft (6);
    irLabel.setBounds (row);

    const int kw = 92, kh = 100;
    auto place = [kw, kh] (Knob* k, int x, int y) { k->setBounds (x, y, kw, kh); };

    // Row 1 - preamp / tone / dynamics
    int y1 = 140;
    place (kInput.get(),    16,  y1);
    place (kGain.get(),     16 + kw,  y1);
    place (kPush.get(),     16 + kw*2, y1);

    place (kBass.get(),     300, y1);
    place (kMid.get(),      300 + 64,  y1);
    place (kTreble.get(),   300 + 128, y1);

    place (kChug.get(),     500, y1);
    place (kLowDrv.get(),   500 + 64,  y1);
    place (kLowMix.get(),   500 + 128, y1);

    // Row 2 - voicing detail + power/output
    int y2 = 280;
    place (kTight.get(),    16,  y2);
    place (kSuperCut.get(), 16 + kw,  y2);
    place (kBias.get(),     16 + kw*2, y2);

    place (kGate.get(),     300, y2);

    place (kSag.get(),      404, y2);
    place (kPower.get(),    404 + 80,  y2);
    place (kMaster.get(),   404 + 160, y2);
}
