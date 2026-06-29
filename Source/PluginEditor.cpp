#include "PluginEditor.h"

ApexAmpEditor::ApexAmpEditor (ApexAmpProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

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

    tuner.getFreq = [this] { return proc.getTunerFrequency(); };
    addAndMakeVisible (tuner);

    meters.getIn  = [this] { return proc.getInputLevel(); };
    meters.getOut = [this] { return proc.getOutputLevel(); };
    addAndMakeVisible (meters);

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

    addAndMakeVisible (loadIRBButton);
    loadIRBButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Select a second IR to blend", juce::File{}, "*.wav;*.aiff;*.aif");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    proc.loadCabIRB (file);
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
    mk (kBlend,    "cabBlend",     "IR Blend");
    mk (kSag,      "sag",          "Sag");
    mk (kPower,    "powerDrive",   "Power");
    mk (kGate,     "gate",         "Gate");
    mk (kPunch,    "outPunch",     "Punch");
    mk (kLoud,     "outLoud",      "Loud");
    mk (kMaster,   "master",       "Master");

    for (auto* k : { kInput.get(), kGain.get(), kPush.get(), kTight.get(), kSuperCut.get(),
                     kBias.get(), kBass.get(), kMid.get(), kTreble.get(), kChug.get(),
                     kLowDrv.get(), kLowMix.get(), kBlend.get(), kSag.get(), kPower.get(),
                     kGate.get(), kPunch.get(), kLoud.get(), kMaster.get() })
        addAndMakeVisible (k);

    updateIRLabel();
    setSize (960, 560);
}

void ApexAmpEditor::layoutRects()
{
    const int M = 14, gap = 12;
    const int W = getWidth();
    rcHeader = { M, 14, W - 2 * M, 60 };

    const int r1y = 82, r2y = 298, rh = 204;
    rcPreamp = { M,                 r1y, 470, rh };
    rcTone   = { M + 470 + gap,     r1y, 196, rh };
    rcDyn    = { M + 470 + 196 + 2 * gap, r1y,
                 W - M - (M + 470 + 196 + 2 * gap), rh };
    rcCab    = { M,                 r2y, 470, rh };
    rcOut    = { M + 470 + gap,     r2y, W - M - (M + 470 + gap), rh };
}

void ApexAmpEditor::updateIRLabel()
{
    const auto a = proc.getCabIRFile();
    const auto b = proc.getCabIRFileB();
    juce::String t;
    if (! a.existsAsFile())
        t = "Built-in cab";
    else
    {
        t = "A: " + a.getFileName();
        if (b.existsAsFile())
            t += "   B: " + b.getFileName();
    }
    irLabel.setText (t, juce::dontSendNotification);
}

ApexAmpEditor::~ApexAmpEditor()
{
    setLookAndFeel (nullptr);
}

void ApexAmpEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    layoutRects();

    // background
    ColourGradient bgGrad (lnf.bg.brighter (0.05f), 0, 0, lnf.bg.darker (0.4f), 0, (float) getHeight(), false);
    g.setGradientFill (bgGrad);
    g.fillAll();

    auto drawPanel = [&] (Rectangle<int> r, const String& title)
    {
        g.setColour (lnf.panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (lnf.line);
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.2f);
        g.setColour (lnf.accent);
        g.setFont (Font (12.0f, Font::bold));
        g.drawText (title, r.getX() + 12, r.getY() + 6, r.getWidth() - 24, 16, Justification::left);
        g.setColour (lnf.accent.withAlpha (0.30f));
        g.fillRect ((float) r.getX() + 12.0f, (float) r.getY() + 24.0f, (float) r.getWidth() - 24.0f, 1.0f);
    };

    // header bar
    g.setColour (lnf.panel);
    g.fillRoundedRectangle (rcHeader.toFloat(), 8.0f);
    g.setColour (lnf.line);
    g.drawRoundedRectangle (rcHeader.toFloat(), 8.0f, 1.2f);

    g.setFont (Font (30.0f, Font::bold));
    g.setColour (Colours::white);
    g.drawText ("APEX", rcHeader.getX() + 16, rcHeader.getY() + 12, 86, 34, Justification::left);
    g.setColour (lnf.accent);
    g.drawText ("AMP", rcHeader.getX() + 100, rcHeader.getY() + 12, 80, 34, Justification::left);
    g.setColour (Colours::white.withAlpha (0.35f));
    g.setFont (Font (10.5f, Font::bold));
    g.drawText ("HIGH-GAIN AMP", rcHeader.getX() + 17, rcHeader.getY() + 40, 160, 12, Justification::left);

    // panels
    drawPanel (rcPreamp, "PREAMP");
    drawPanel (rcTone,   "TONE");
    drawPanel (rcDyn,    "DYNAMICS");
    drawPanel (rcCab,    "CABINET");
    drawPanel (rcOut,    "POWER / OUTPUT");

    // footer
    g.setColour (Colours::white.withAlpha (0.3f));
    g.setFont (Font (11.0f));
    g.drawText ("ApexAmp  v0.2 beta", 16, getHeight() - 24, 300, 16, Justification::left);
    g.drawText ("PolychromeNext", getWidth() - 216, getHeight() - 24, 200, 16, Justification::right);
}

void ApexAmpEditor::resized()
{
    layoutRects();

    // --- header controls ---
    presetBox.setBounds        (rcHeader.getX() + 200, rcHeader.getY() + 17, 176, 26);
    savePresetButton.setBounds (rcHeader.getX() + 382, rcHeader.getY() + 17, 54, 26);
    loadPresetButton.setBounds (rcHeader.getX() + 440, rcHeader.getY() + 17, 54, 26);
    meters.setBounds           (rcHeader.getX() + 504, rcHeader.getY() + 13, 168, 34);
    tuner.setBounds            (rcHeader.getRight() - 258, rcHeader.getY() + 13, 246, 34);

    // Place a horizontal row of knobs inside a panel's content area.
    auto knobRow = [] (juce::Rectangle<int> panel, std::vector<Knob*> knobs)
    {
        auto content = panel.reduced (10);
        content.removeFromTop (24);              // title band
        const int n = (int) knobs.size();
        if (n == 0) return;
        const int w = content.getWidth() / n;
        for (int i = 0; i < n; ++i)
            knobs[(size_t) i]->setBounds (content.getX() + i * w, content.getY(), w, content.getHeight());
    };

    // combos sit in panel title bands (right side)
    channelBox.setBounds   (rcPreamp.getRight() - 120, rcPreamp.getY() + 4, 108, 20);
    tonestackBox.setBounds (rcTone.getRight()  - 124, rcTone.getY()  + 4, 116, 20);

    knobRow (rcPreamp, { kInput.get(), kGain.get(), kPush.get(), kTight.get(), kSuperCut.get(), kBias.get() });
    knobRow (rcTone,   { kBass.get(), kMid.get(), kTreble.get() });
    knobRow (rcDyn,    { kGate.get(), kChug.get(), kLowDrv.get(), kLowMix.get() });
    knobRow (rcOut,    { kSag.get(), kPower.get(), kPunch.get(), kLoud.get(), kMaster.get() });

    // --- cabinet panel (combo + buttons + IR label + blend knob) ---
    {
        auto cab = rcCab.reduced (10);
        cab.removeFromTop (24); // title

        auto blendArea = cab.removeFromRight (96);
        kBlend->setBounds (blendArea.withSizeKeepingCentre (96, juce::jmin (blendArea.getHeight(), 116)));
        cab.removeFromRight (8);

        auto row1 = cab.removeFromTop (26);
        cabTypeBox.setBounds (row1.removeFromLeft (158));
        row1.removeFromLeft (8);
        cabButton.setBounds (row1.removeFromLeft (60));

        cab.removeFromTop (8);
        auto row2 = cab.removeFromTop (26);
        loadIRButton.setBounds  (row2.removeFromLeft (74)); row2.removeFromLeft (6);
        loadIRBButton.setBounds (row2.removeFromLeft (74)); row2.removeFromLeft (6);
        clearIRButton.setBounds (row2.removeFromLeft (78));

        cab.removeFromTop (8);
        irLabel.setBounds (cab.removeFromTop (40));
    }
}
