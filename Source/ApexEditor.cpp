#include "ApexEditor.h"

//==============================================================================
const juce::Colour ApexLookAndFeel::accent   { 0xffff7a18 }; // molten amber
const juce::Colour ApexLookAndFeel::accent2  { 0xff39c0ff }; // cyan (meters)
const juce::Colour ApexLookAndFeel::panelTop { 0xff20232a };
const juce::Colour ApexLookAndFeel::panelBot { 0xff121317 };
const juce::Colour ApexLookAndFeel::border   { 0xff32363f };
const juce::Colour ApexLookAndFeel::text     { 0xffe7e9ee };
const juce::Colour ApexLookAndFeel::textDim  { 0xff8a8f99 };

ApexLookAndFeel::ApexLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,    text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0c0d10));
    setColour (juce::ComboBox::backgroundColourId,   juce::Colour (0xff0c0d10));
    setColour (juce::ComboBox::textColourId,         text);
    setColour (juce::ComboBox::outlineColourId,      border);
    setColour (juce::ComboBox::arrowColourId,        accent);
    setColour (juce::PopupMenu::backgroundColourId,  juce::Colour (0xff15171c));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.30f));
    setColour (juce::PopupMenu::textColourId,        text);
}

juce::Font ApexLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (14.0f, juce::Font::bold));
}

juce::Font ApexLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (12.0f, juce::Font::bold));
}

void ApexLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void ApexLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                        float sliderPos, float startAngle, float endAngle,
                                        juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const auto dia = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto knob = juce::Rectangle<float> (bounds.getCentreX() - dia * 0.5f,
                                              bounds.getCentreY() - dia * 0.5f, dia, dia).reduced (4.0f);
    const auto radius = knob.getWidth() * 0.5f;
    const auto centre = knob.getCentre();
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);

    // outer drop shadow
    g.setColour (juce::Colour (0xff05060a).withAlpha (0.6f));
    g.fillEllipse (knob.translated (0.0f, 2.5f).expanded (3.0f));

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius + 5.0f, radius + 5.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a2e36));
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius + 5.0f, radius + 5.0f, 0.0f, startAngle, angle, true);
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a4150), knob.getX(), knob.getY(),
                                             juce::Colour (0xff0e1014), knob.getRight(), knob.getBottom(), false));
    g.fillEllipse (knob);
    g.setColour (juce::Colour (0xff464d59).withAlpha (0.55f));
    g.drawEllipse (knob.reduced (0.5f), 1.2f);
    g.setColour (juce::Colour (0xff05060a).withAlpha (0.5f));
    g.fillEllipse (knob.reduced (radius * 0.42f));

    // pointer
    juce::Path pointer;
    const auto len = radius * 0.74f;
    const auto thick = juce::jmax (2.0f, radius * 0.10f);
    pointer.addRoundedRectangle (-thick * 0.5f, -len, thick, len * 0.6f, thick * 0.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (accent.brighter (0.4f));
    g.fillPath (pointer);

    if (slider.isMouseOverOrDragging())
    {
        g.setColour (accent.withAlpha (0.14f));
        g.fillEllipse (knob.expanded (4.0f));
    }
}

void ApexLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                        bool highlighted, bool /*down*/)
{
    auto bounds = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto on = b.getToggleState();
    g.setColour (on ? accent.withAlpha (0.22f) : juce::Colour (0xff0c0d10));
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (on ? accent : border.withAlpha (highlighted ? 0.95f : 0.65f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, on ? 1.6f : 1.0f);

    // LED
    auto led = bounds.removeFromLeft (22.0f).withSizeKeepingCentre (10.0f, 10.0f);
    g.setColour (on ? accent : juce::Colour (0xff3a3f49));
    g.fillEllipse (led);
    if (on)
    {
        g.setColour (accent.withAlpha (0.3f));
        g.fillEllipse (led.expanded (3.0f));
    }

    g.setColour (on ? text : textDim);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (b.getButtonText(), bounds.reduced (4, 0), juce::Justification::centred);
}

void ApexLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox&)
{
    const auto bounds = juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
    g.setColour (juce::Colour (0xff0c0d10));
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (border);
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

    juce::Path arrow;
    const auto cx = bounds.getRight() - 15.0f;
    const auto cy = bounds.getCentreY();
    arrow.startNewSubPath (cx - 5.0f, cy - 3.0f);
    arrow.lineTo (cx, cy + 3.0f);
    arrow.lineTo (cx + 5.0f, cy - 3.0f);
    g.setColour (accent);
    g.strokePath (arrow, juce::PathStrokeType (2.0f));
}

//==============================================================================
ApexAmpEditor::ApexAmpEditor (ApexAmpProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    inputGain  = &addKnob ("inputGain",  "INPUT",     " dB", 1);
    gateThresh = &addKnob ("gate",       "THRESH",    " dB", 0);
    gateHold   = &addKnob ("gateHold",   "HOLD",      " ms", 0);
    mixBite    = &addKnob ("mixBite",    "BITE",      "",    2);
    mixBody    = &addKnob ("mixBody",    "BODY",      "",    2);
    mixEdge    = &addKnob ("mixEdge",    "EDGE",      "",    2);
    cabMix     = &addKnob ("cabMix",     "CAB MIX",   " %",  0);
    presence   = &addKnob ("presence",   "PRESENCE",  " dB", 1);
    lowCut     = &addKnob ("lowCut",     "LOW CUT",   " Hz", 0);
    outputGain = &addKnob ("outputGain", "OUTPUT",    " dB", 1);

    auto setupCombo = [this] (juce::ComboBox& box, juce::Label& label, const juce::String& title,
                              const juce::StringArray& items, const juce::String& paramID,
                              std::unique_ptr<CA>& att)
    {
        label.setText (title, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, ApexLookAndFeel::textDim);
        label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        label.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);

        for (int i = 0; i < items.size(); ++i)
            box.addItem (items[i], i + 1);
        addAndMakeVisible (box);
        att = std::make_unique<CA> (proc.apvts, paramID, box);
    };

    setupCombo (rigModeBox, rigModeLabel, "MODE", { "Single", "Blend" }, "rigMode", rigModeAtt);
    setupCombo (rigBox,     rigLabel,     "RIG",  { "Bite", "Body", "Edge" }, "rig", rigAtt);
    setupCombo (irBox,      irLabel,      "CABINET", { "Ashen", "Meshuggah", "PDI-09" }, "ir", irAtt);

    addAndMakeVisible (gateButton);
    gateButton.setClickingTogglesState (true);
    gateAtt = std::make_unique<BA> (proc.apvts, "gateOn", gateButton);

    setSize (960, 560);
    startTimerHz (30);
}

ApexAmpEditor::~ApexAmpEditor()
{
    setLookAndFeel (nullptr);
}

ApexAmpEditor::Knob& ApexAmpEditor::addKnob (const juce::String& paramID, const juce::String& name,
                                             const juce::String& suffix, int decimals)
{
    auto* k = new Knob();
    knobs.add (k);

    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k->slider.setColour (juce::Slider::textBoxTextColourId, ApexLookAndFeel::text);
    if (suffix.isNotEmpty())
        k->slider.setTextValueSuffix (suffix);
    k->slider.setNumDecimalPlacesToDisplay (decimals);
    addAndMakeVisible (k->slider);

    k->nameLabel.setText (name, juce::dontSendNotification);
    k->nameLabel.setColour (juce::Label::textColourId, ApexLookAndFeel::textDim);
    k->nameLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    k->nameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k->nameLabel);

    k->att = std::make_unique<SA> (proc.apvts, paramID, k->slider);
    return *k;
}

//==============================================================================
void ApexAmpEditor::drawHeader (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff23262e), (float) r.getX(), (float) r.getY(),
                                             juce::Colour (0xff0c0d10), (float) r.getX(), (float) r.getBottom(), false));
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (ApexLookAndFeel::border);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);

    auto inner = r.reduced (18, 0);
    g.setColour (ApexLookAndFeel::text);
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.drawText ("APEX", inner.removeFromLeft (88).toFloat(), juce::Justification::centredLeft);
    g.setColour (ApexLookAndFeel::accent);
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.drawText ("AMP", inner.removeFromLeft (78).toFloat(), juce::Justification::centredLeft);

    g.setColour (ApexLookAndFeel::textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText ("NEURAL CAPTURE ENGINE", inner.removeFromLeft (220).toFloat(),
                juce::Justification::centredLeft);
}

void ApexAmpEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    g.setGradientFill (juce::ColourGradient (ApexLookAndFeel::panelTop, (float) r.getX(), (float) r.getY(),
                                             ApexLookAndFeel::panelBot, (float) r.getX(), (float) r.getBottom(), false));
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (ApexLookAndFeel::border);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);

    auto titleArea = r.reduced (14, 10).removeFromTop (18);
    g.setColour (ApexLookAndFeel::accent);
    g.fillRect (titleArea.removeFromLeft (3).withTrimmedTop (2).withTrimmedBottom (2));
    g.setColour (ApexLookAndFeel::text);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (title, titleArea.translated (8, 0), juce::Justification::centredLeft);
}

void ApexAmpEditor::drawMeter (juce::Graphics& g, juce::Rectangle<int> r, float level01, const juce::String& label)
{
    g.setColour (juce::Colour (0xff0a0b0e));
    g.fillRoundedRectangle (r.toFloat(), 4.0f);
    g.setColour (ApexLookAndFeel::border);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 4.0f, 1.0f);

    auto bar = r.reduced (4).withTrimmedBottom (16);
    const int filledH = (int) (level01 * (float) bar.getHeight());
    auto filled = bar.removeFromBottom (filledH);

    juce::Colour c = level01 > 0.92f ? juce::Colour (0xffff4d4d)
                   : level01 > 0.75f ? juce::Colour (0xffffc24d)
                                     : ApexLookAndFeel::accent2;
    g.setColour (c);
    g.fillRoundedRectangle (filled.toFloat(), 2.0f);

    g.setColour (ApexLookAndFeel::textDim);
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText (label, r.removeFromBottom (14), juce::Justification::centred);
}

void ApexAmpEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff16181d), 0.0f, 0.0f,
                                             juce::Colour (0xff0a0b0e), 0.0f, (float) getHeight(), false));
    g.fillAll();

    auto area = getLocalBounds().reduced (16);
    drawHeader (g, area.removeFromTop (72));

    drawPanel (g, inputPanel, "INPUT / GATE");
    drawPanel (g, rigPanel,   "RIG");
    drawPanel (g, cabPanel,   "CABINET");
    drawPanel (g, outPanel,   "OUTPUT");

    // meters in the output panel (right side)
    auto meters = outPanel.reduced (14, 0);
    meters.removeFromTop (170);
    meters = meters.withTrimmedBottom (12);
    auto inM = meters.removeFromLeft (meters.getWidth() / 2 - 4);
    meters.removeFromLeft (8);
    drawMeter (g, inM, inMeter, "IN");
    drawMeter (g, meters, outMeter, "OUT");
}

//==============================================================================
void ApexAmpEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (72 + 12); // header + gap

    const int gap = 12;
    auto row = area;

    inputPanel = row.removeFromLeft (180); row.removeFromLeft (gap);
    rigPanel   = row.removeFromLeft (300); row.removeFromLeft (gap);
    cabPanel   = row.removeFromLeft (300); row.removeFromLeft (gap);
    outPanel   = row;

    auto placeKnob = [] (Knob& k, juce::Rectangle<int> cell)
    {
        k.nameLabel.setBounds (cell.removeFromTop (16));
        k.slider.setBounds (cell);
    };

    // ----- INPUT / GATE panel -----
    {
        auto p = inputPanel.reduced (14, 14);
        p.removeFromTop (22); // title
        placeKnob (*inputGain, p.removeFromTop (132));
        p.removeFromTop (6);
        gateButton.setBounds (p.removeFromTop (30));
        p.removeFromTop (6);
        auto g2 = p.removeFromTop (110);
        placeKnob (*gateThresh, g2.removeFromLeft (g2.getWidth() / 2).reduced (2, 0));
        placeKnob (*gateHold,   g2.reduced (2, 0));
    }

    // ----- RIG panel -----
    {
        auto p = rigPanel.reduced (14, 14);
        p.removeFromTop (22);
        auto combos = p.removeFromTop (50);
        auto cMode = combos.removeFromLeft (combos.getWidth() / 2).reduced (2, 0);
        auto cRig  = combos.reduced (2, 0);
        rigModeLabel.setBounds (cMode.removeFromTop (14));
        rigModeBox.setBounds (cMode.removeFromTop (30));
        rigLabel.setBounds (cRig.removeFromTop (14));
        rigBox.setBounds (cRig.removeFromTop (30));

        p.removeFromTop (10);
        auto knobRow = p.removeFromTop (148);
        const int kw = knobRow.getWidth() / 3;
        placeKnob (*mixBite, knobRow.removeFromLeft (kw).reduced (4, 0));
        placeKnob (*mixBody, knobRow.removeFromLeft (kw).reduced (4, 0));
        placeKnob (*mixEdge, knobRow.reduced (4, 0));
    }

    // ----- CABINET panel -----
    {
        auto p = cabPanel.reduced (14, 14);
        p.removeFromTop (22);
        irLabel.setBounds (p.removeFromTop (14));
        irBox.setBounds (p.removeFromTop (30));
        p.removeFromTop (12);
        auto knobRow = p.removeFromTop (148);
        const int kw = knobRow.getWidth() / 3;
        placeKnob (*cabMix,   knobRow.removeFromLeft (kw).reduced (4, 0));
        placeKnob (*presence, knobRow.removeFromLeft (kw).reduced (4, 0));
        placeKnob (*lowCut,   knobRow.reduced (4, 0));
    }

    // ----- OUTPUT panel -----
    {
        auto p = outPanel.reduced (14, 14);
        p.removeFromTop (22);
        placeKnob (*outputGain, p.removeFromTop (132));
        // meters drawn in paint() below this
    }
}

//==============================================================================
void ApexAmpEditor::timerCallback()
{
    auto smooth = [] (float& state, float target)
    {
        // fast attack, slow release for a musical meter feel
        const float coef = target > state ? 0.6f : 0.12f;
        state += coef * (target - state);
    };

    // map magnitude (linear) to a 0..1 meter using a -60..0 dB scale
    auto toMeter = [] (float mag)
    {
        const float db = juce::Decibels::gainToDecibels (mag, -60.0f);
        return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    };

    smooth (inMeter,  toMeter (proc.inputMagnitude.load()));
    smooth (outMeter, toMeter (proc.outputMagnitude.load()));
    repaint();
}
