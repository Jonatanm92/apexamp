#include "PluginEditor.h"

ApexAmpEditor::ApexAmpEditor (ApexAmpProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    auto& apvts = proc.apvts;

    // --- selectors ---
    channelBox.addItemList ({ "Tight", "Scoop" }, 1);
    addAndMakeVisible (channelBox);
    channelAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, "channel", channelBox);

    tonestackBox.addItemList ({ "Marshall", "Fender", "Mesa", "Modern Metal" }, 1);
    addAndMakeVisible (tonestackBox);
    tonestackAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, "tonestack", tonestackBox);

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
                    proc.loadCabIR (file);
            });
    };

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
    mk (kMaster,   "master",       "Master");

    for (auto* k : { kInput.get(), kGain.get(), kPush.get(), kTight.get(), kSuperCut.get(),
                     kBias.get(), kBass.get(), kMid.get(), kTreble.get(), kChug.get(),
                     kLowDrv.get(), kLowMix.get(), kSag.get(), kPower.get(), kMaster.get() })
        addAndMakeVisible (k);

    setSize (760, 420);
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
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (40); // title

    // top selector row
    auto top = area.removeFromTop (60);
    channelBox.setBounds   (top.removeFromLeft (140).withSizeKeepingCentre (140, 26));
    top.removeFromLeft (10);
    tonestackBox.setBounds (top.removeFromLeft (160).withSizeKeepingCentre (160, 26));
    top.removeFromLeft (20);
    cabButton.setBounds    (top.removeFromLeft (70).withSizeKeepingCentre (70, 26));
    loadIRButton.setBounds (top.removeFromLeft (110).withSizeKeepingCentre (110, 26));

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

    place (kSag.get(),      400, y2);
    place (kPower.get(),    400 + 92,  y2);
    place (kMaster.get(),   400 + 184, y2);
}
