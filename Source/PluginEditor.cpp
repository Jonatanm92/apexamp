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

    // ---- background: gradient + faint brushed lines + frame ----
    ColourGradient bgGrad (Colour (0xff1a1d24), 0, 0, Colour (0xff0c0d11), 0, (float) getHeight(), false);
    g.setGradientFill (bgGrad);
    g.fillAll();
    g.setColour (Colours::white.withAlpha (0.012f));
    for (int yy = 0; yy < getHeight(); yy += 3)
        g.drawHorizontalLine (yy, 0.0f, (float) getWidth());

    auto drawPanel = [&] (Rectangle<int> r, const String& title)
    {
        auto rf = r.toFloat();
        g.setColour (Colours::black.withAlpha (0.40f));
        g.fillRoundedRectangle (rf.translated (0.0f, 3.0f).expanded (1.0f), 10.0f);
        ColourGradient pg (lnf.panelHi, rf.getX(), rf.getY(), lnf.panel.darker (0.28f), rf.getX(), rf.getBottom(), false);
        g.setGradientFill (pg);
        g.fillRoundedRectangle (rf, 9.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawLine (rf.getX() + 8.0f, rf.getY() + 2.0f, rf.getRight() - 8.0f, rf.getY() + 2.0f, 1.0f);
        g.setColour (lnf.line);
        g.drawRoundedRectangle (rf, 9.0f, 1.3f);
        g.setColour (lnf.accent);
        g.fillRoundedRectangle ((float) r.getX() + 14.0f, (float) r.getY() + 11.0f, 4.0f, 13.0f, 1.5f);
        g.setColour (lnf.text);
        g.setFont (Font (12.5f, Font::bold));
        g.drawText (title, r.getX() + 24, r.getY() + 8, r.getWidth() - 34, 16, Justification::left);
        ApexLookAndFeel::drawScrew (g, (float) r.getX() + 11.0f,     (float) r.getY() + 11.0f, 4.0f);
        ApexLookAndFeel::drawScrew (g, (float) r.getRight() - 11.0f, (float) r.getY() + 11.0f, 4.0f);
    };

    // ---- header: brushed metal bar with screws + branded wordmark ----
    {
        auto rf = rcHeader.toFloat();
        g.setColour (Colours::black.withAlpha (0.40f));
        g.fillRoundedRectangle (rf.translated (0.0f, 3.0f).expanded (1.0f), 10.0f);
        ColourGradient hg (Colour (0xff343b46), rf.getX(), rf.getY(), Colour (0xff1c2027), rf.getX(), rf.getBottom(), false);
        g.setGradientFill (hg);
        g.fillRoundedRectangle (rf, 9.0f);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawLine (rf.getX() + 8.0f, rf.getY() + 2.0f, rf.getRight() - 8.0f, rf.getY() + 2.0f, 1.0f);
        g.setColour (lnf.line);
        g.drawRoundedRectangle (rf, 9.0f, 1.3f);

        ApexLookAndFeel::drawScrew (g, rf.getX() + 12.0f,     rf.getY() + 12.0f, 4.5f);
        ApexLookAndFeel::drawScrew (g, rf.getRight() - 12.0f, rf.getY() + 12.0f, 4.5f);
        ApexLookAndFeel::drawScrew (g, rf.getX() + 12.0f,     rf.getBottom() - 12.0f, 4.5f);
        ApexLookAndFeel::drawScrew (g, rf.getRight() - 12.0f, rf.getBottom() - 12.0f, 4.5f);

        const int lx = rcHeader.getX() + 22, ly = rcHeader.getY() + 10;
        g.setFont (Font (31.0f, Font::bold));
        g.setColour (Colours::white);
        g.drawText ("APEX", lx, ly, 88, 34, Justification::left);
        g.setColour (lnf.accent);
        g.drawText ("AMP", lx + 92, ly, 86, 34, Justification::left);
        g.setColour (lnf.accent);
        g.fillRoundedRectangle ((float) lx + 1.0f, (float) ly + 35.0f, 168.0f, 2.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.setFont (Font (9.5f, Font::bold));
        g.drawText ("HIGH-GAIN AMPLIFIER", lx + 1, ly + 39, 220, 12, Justification::left);
    }

    drawPanel (rcPreamp, "PREAMP");
    drawPanel (rcTone,   "TONE");
    drawPanel (rcDyn,    "DYNAMICS");
    drawPanel (rcCab,    "CABINET");
    drawPanel (rcOut,    "POWER / OUTPUT");

    // footer
    g.setColour (Colours::white.withAlpha (0.28f));
    g.setFont (Font (11.0f));
    g.drawText ("ApexAmp  v0.2 beta", 18, getHeight() - 22, 300, 16, Justification::left);
    g.drawText ("PolychromeNext", getWidth() - 216, getHeight() - 22, 200, 16, Justification::right);
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
