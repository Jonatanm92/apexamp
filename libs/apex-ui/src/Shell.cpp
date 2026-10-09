#include "apex/ui/Shell.h"
#include "apex/ui/Materials.h"
#include "apex/ui/Theme.h"

#include <cmath>
#include <cstring>

namespace apex::ui
{

//==============================================================================
IconButton::IconButton (Icon i, const juce::String& tip) : juce::Button (tip), icon (i)
{
    setTooltip (tip);
    setTitle (tip);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Path IconButton::pathFor (Icon icon)
{
    // 24 x 24 design grid, stroked.
    juce::Path p;
    switch (icon)
    {
        case Icon::previous: p.startNewSubPath (14.5f, 6.0f); p.lineTo (8.5f, 12.0f); p.lineTo (14.5f, 18.0f); break;
        case Icon::next:     p.startNewSubPath (9.5f, 6.0f);  p.lineTo (15.5f, 12.0f); p.lineTo (9.5f, 18.0f); break;
        case Icon::save:
            p.startNewSubPath (5.0f, 14.0f); p.lineTo (5.0f, 19.0f); p.lineTo (19.0f, 19.0f); p.lineTo (19.0f, 14.0f);
            p.startNewSubPath (12.0f, 4.5f); p.lineTo (12.0f, 14.5f);
            p.startNewSubPath (8.0f, 10.5f); p.lineTo (12.0f, 14.5f); p.lineTo (16.0f, 10.5f);
            break;
        case Icon::undo:
            p.startNewSubPath (9.0f, 14.0f); p.lineTo (4.5f, 9.5f); p.lineTo (9.0f, 5.0f);
            p.startNewSubPath (4.5f, 9.5f); p.lineTo (14.5f, 9.5f);
            p.quadraticTo (19.5f, 9.5f, 19.5f, 14.25f); p.quadraticTo (19.5f, 19.0f, 14.5f, 19.0f); p.lineTo (11.5f, 19.0f);
            break;
        case Icon::redo:
            p.startNewSubPath (15.0f, 14.0f); p.lineTo (19.5f, 9.5f); p.lineTo (15.0f, 5.0f);
            p.startNewSubPath (19.5f, 9.5f); p.lineTo (9.5f, 9.5f);
            p.quadraticTo (4.5f, 9.5f, 4.5f, 14.25f); p.quadraticTo (4.5f, 19.0f, 9.5f, 19.0f); p.lineTo (12.5f, 19.0f);
            break;
        case Icon::tuner:
            // tuning fork
            p.startNewSubPath (8.5f, 3.5f); p.lineTo (8.5f, 10.5f);
            p.quadraticTo (8.5f, 14.0f, 12.0f, 14.0f); p.quadraticTo (15.5f, 14.0f, 15.5f, 10.5f); p.lineTo (15.5f, 3.5f);
            p.startNewSubPath (12.0f, 14.0f); p.lineTo (12.0f, 20.5f);
            break;
        case Icon::settings:
            p.startNewSubPath (4.0f, 7.0f);  p.lineTo (13.0f, 7.0f);
            p.startNewSubPath (17.0f, 7.0f); p.lineTo (20.0f, 7.0f);
            p.addEllipse (13.0f, 5.0f, 4.0f, 4.0f);
            p.startNewSubPath (4.0f, 17.0f); p.lineTo (7.0f, 17.0f);
            p.startNewSubPath (11.0f, 17.0f); p.lineTo (20.0f, 17.0f);
            p.addEllipse (7.0f, 15.0f, 4.0f, 4.0f);
            break;
        case Icon::close:
            p.startNewSubPath (6.5f, 6.5f); p.lineTo (17.5f, 17.5f);
            p.startNewSubPath (17.5f, 6.5f); p.lineTo (6.5f, 17.5f);
            break;
    }
    return p;
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat();
    if (over || getToggleState())
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.10f : 0.05f));
        g.fillRoundedRectangle (b.reduced (1.0f), 6.0f);
    }
    const float s = juce::jmin (b.getWidth(), b.getHeight()) * 0.78f;
    auto p = pathFor (icon);
    p.applyTransform (juce::AffineTransform::scale (s / 24.0f).translated (b.getCentreX() - s * 0.5f, b.getCentreY() - s * 0.5f));
    const auto colour = getToggleState() ? colours::amber
                      : ! isEnabled()    ? colours::faint
                      : over             ? colours::bone : colours::label;
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
PresetBox::PresetBox() : juce::Button ("Presets")
{
    setTooltip ("Presets");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PresetBox::setPreset (const juce::String& n, bool m)
{
    if (n != name || m != modified)
    {
        name = n;
        modified = m;
        repaint();
    }
}

void PresetBox::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (down ? 0xff111113 : 0xff0a0a0b));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (over ? colours::hairline.brighter (0.4f) : colours::hairline);
    g.drawRoundedRectangle (b, 6.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (b.getX() + 6.0f, b.getY() + 1.2f, b.getRight() - 6.0f, b.getY() + 1.2f, 1.0f);

    auto text = b.reduced (14.0f, 0.0f);
    auto chevronArea = text.removeFromRight (14.0f);
    const auto font = Fonts::label (17.0f, 0.1f);
    g.setFont (font);
    g.setColour (colours::bone);
    const auto shown = name.toUpperCase();
    g.drawText (shown, text, juce::Justification::centredLeft, true);

    if (modified)
    {
        const float tw = juce::jmin (text.getWidth() - 10.0f, juce::GlyphArrangement::getStringWidth (font, shown));
        g.setColour (colours::amber);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ text.getX() + tw + 10.0f, text.getCentreY() }));
    }

    juce::Path chevron;
    const auto c = chevronArea.getCentre();
    chevron.startNewSubPath (c.x - 4.0f, c.y - 2.0f);
    chevron.lineTo (c.x, c.y + 2.5f);
    chevron.lineTo (c.x + 4.0f, c.y - 2.0f);
    g.setColour (colours::dim);
    g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
HeaderBar::HeaderBar (const juce::String& productName) : product (productName)
{
    for (auto* c : std::initializer_list<juce::Component*> { &previousButton, &presetBox, &nextButton, &saveButton,
                                                             &aButton, &bButton, &undoButton, &redoButton,
                                                             &inMeter, &outMeter, &tunerButton, &settingsButton })
        addAndMakeVisible (c);

    aButton.setClickingTogglesState (false);
    bButton.setClickingTogglesState (false);
    aButton.setTooltip ("Compare: setting A");
    bButton.setTooltip ("Compare: setting B");
    aButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    bButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    inMeter.setTitle ("Input level");
    outMeter.setTitle ("Output level");

    addChildComponent (autoButton);
    autoButton.setClickingTogglesState (false);
    autoButton.setTooltip ("Auto Input: play for a few seconds and the input level is set for you");
}

void HeaderBar::setShowsAutoInput (bool shouldShow)
{
    showsAutoInput = shouldShow;
    autoButton.setVisible (shouldShow);
    resized();
}

void HeaderBar::setShowsTuner (bool shouldShow)
{
    showsTuner = shouldShow;
    tunerButton.setVisible (shouldShow);
    resized();
}

void HeaderBar::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff131315), 0.0f, 0.0f, colours::headerBg, 0.0f, b.getBottom(), false));
    g.fillRect (b);
    g.setColour (colours::hairline);
    g.fillRect (b.removeFromBottom (1.0f));
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRect (getLocalBounds().toFloat().removeFromTop (1.0f));

    auto left = getLocalBounds().toFloat().withTrimmedLeft (22.0f);
    const auto wordFont = Fonts::display (25.0f, 0.26f);
    g.setFont (wordFont);
    g.setColour (colours::bone);
    g.drawText ("APEX", left, juce::Justification::centredLeft, false);
    const float ww = juce::GlyphArrangement::getStringWidth (wordFont, "APEX");
    g.setFont (Fonts::label (13.0f, 0.34f));
    g.setColour (colours::dim);
    g.drawText (product.toUpperCase(), left.withTrimmedLeft (ww + 12.0f), juce::Justification::centredLeft, false);

    if (! meterArea.isEmpty())
    {
        g.setFont (Fonts::label (10.0f, 0.24f));
        g.setColour (colours::dim);
        g.drawText ("IN",  inMeter.getBounds().translated (-24, 0).withWidth (22), juce::Justification::centredRight, false);
        g.drawText ("OUT", outMeter.getBounds().translated (-24, 0).withWidth (22), juce::Justification::centredRight, false);
    }
}

void HeaderBar::resized()
{
    auto b = getLocalBounds();
    const int h = b.getHeight();
    const int bh = 32, y = (h - bh) / 2;
    const bool compact = b.getWidth() < 1000;

    // right group: settings, tuner, meters
    int r = b.getRight() - 14;
    settingsButton.setBounds (r - 32, y, 32, bh); r -= 36;
    if (showsTuner)
    {
        tunerButton.setBounds (r - 32, y, 32, bh);
        r -= 40;
    }
    const int meterW = compact ? 64 : 96;
    r -= 6;
    outMeter.setBounds (r - meterW, h / 2 + 3, meterW, 7);
    inMeter.setBounds (r - meterW, h / 2 - 10, meterW, 7);
    meterArea = { r - meterW - 26, 0, meterW + 26, h };
    int rightEdge = meterArea.getX() - 12;
    if (showsAutoInput)
    {
        const int aw = compact ? 46 : 54;
        autoButton.setBounds (rightEdge - aw, y + 3, aw, bh - 6);
        rightEdge -= aw + 12;
    }

    // centre group, shrinking the preset box to fit
    const int leftEdge = compact ? 150 : 190;
    const int fixed = 32 + 6 + 6 + 32 + 4 + 32 + (compact ? 12 : 18) + 76 + (compact ? 10 : 14) + 32 + 2 + 32;
    const int presetW = juce::jlimit (150, 270, rightEdge - leftEdge - fixed);
    const int groupW = fixed + presetW;
    int x = juce::jmax (leftEdge, juce::jmin ((b.getWidth() - groupW) / 2, rightEdge - groupW));
    previousButton.setBounds (x, y, 32, bh);   x += 38;
    presetBox.setBounds (x, y, presetW, bh);   x += presetW + 6;
    nextButton.setBounds (x, y, 32, bh);       x += 36;
    saveButton.setBounds (x, y, 32, bh);       x += 32 + (compact ? 12 : 18);
    aButton.setBounds (x, y + 2, 38, bh - 4);  x += 38;
    bButton.setBounds (x, y + 2, 38, bh - 4);  x += 38 + (compact ? 10 : 14);
    undoButton.setBounds (x, y, 32, bh);       x += 34;
    redoButton.setBounds (x, y, 32, bh);
}

//==============================================================================
TunerTheme TunerTheme::apex()
{
    TunerTheme t;
    t.accent = colours::bone;
    t.inTune = colours::amber;
    t.text = colours::bone;
    t.dim = colours::dim;
    t.faint = colours::faint;
    t.panelTop = juce::Colour (0xff141416);
    t.panelBottom = juce::Colour (0xff09090a);
    t.outline = colours::hairline;
    t.noteFont = [] (float h) { return Fonts::display (h, 0.02f); };
    t.labelFont = [] (float h) { return Fonts::label (h, 0.34f); };
    t.readoutFont = [] (float h) { return Fonts::mono (h); };
    return t;
}

TunerOverlay::TunerOverlay (TunerFeed& f) : feed (f)
{
    history.assign (2048, 0.0f);
    scratch.assign (4096, 0.0f);
    diff.assign (1024, 0.0f);

    addAndMakeVisible (muteButton);
    addAndMakeVisible (closeButton);
    muteButton.setClickingTogglesState (true);
    muteButton.setToggleState (true, juce::dontSendNotification);
    muteButton.setTooltip ("Silence the plugin output while tuning");
    muteButton.onClick = [this]
    {
        muteOutput = muteButton.getToggleState();
        feed.mute.store (muteOutput && isVisible());
    };
    closeButton.onClick = [this] { close(); };
    setWantsKeyboardFocus (true);
}

TunerOverlay::~TunerOverlay()
{
    feed.active.store (false);
    feed.mute.store (false);
}

void TunerOverlay::open()
{
    setVisible (true);
    toFront (true);
}

void TunerOverlay::close()
{
    setVisible (false);
    if (onClose)
        onClose();
}

void TunerOverlay::visibilityChanged()
{
    const bool showing = isVisible();
    feed.active.store (showing);
    feed.mute.store (showing && muteOutput);
    if (showing)
    {
        historyFill = 0;
        note = -1;
        startTimerHz (30);
    }
    else
    {
        stopTimer();
    }
}

void TunerOverlay::mouseDown (const juce::MouseEvent& e)
{
    // Click outside the panel closes the tuner.
    const auto panel = getLocalBounds().withSizeKeepingCentre (660, 400);
    if (! panel.contains (e.getPosition()))
        close();
}

void TunerOverlay::resized()
{
    const auto panel = getLocalBounds().withSizeKeepingCentre (660, 400);
    closeButton.setBounds (panel.getRight() - 44, panel.getY() + 12, 32, 32);
    muteButton.setBounds (panel.getCentreX() - 80, panel.getBottom() - 54, 160, 32);
}

float TunerOverlay::detectPitch()
{
    const double sr = feed.getRate();
    const int maxLag = juce::jmin ((int) diff.size() - 2, (int) (sr / 27.0));
    const int minLag = juce::jmax (2, (int) (sr / 1400.0));
    const int window = 1024;
    const int needed = window + maxLag + 2;
    if (historyFill < needed)
        return 0.0f;

    const float* x = history.data() + (history.size() - (size_t) needed);
    double energy = 0.0;
    for (int i = 0; i < window; ++i)
        energy += (double) x[i] * x[i];
    if (energy / window < 2.0e-7)
        return 0.0f;

    double running = 0.0;
    diff[0] = 1.0f;
    for (int tau = 1; tau <= maxLag + 1; ++tau)
    {
        double acc = 0.0;
        for (int j = 0; j < window; ++j)
        {
            const double dlt = (double) x[j] - (double) x[j + tau];
            acc += dlt * dlt;
        }
        running += acc;
        diff[(size_t) tau] = running > 0.0 ? (float) (acc * tau / running) : 1.0f;
    }

    int tau = -1;
    for (int t = minLag; t <= maxLag; ++t)
        if (diff[(size_t) t] < 0.12f)
        {
            while (t + 1 <= maxLag && diff[(size_t) t + 1] < diff[(size_t) t])
                ++t;
            tau = t;
            break;
        }
    if (tau < 0)
        return 0.0f;

    const float a = diff[(size_t) tau - 1], b = diff[(size_t) tau], c = diff[(size_t) tau + 1];
    const float denom = a - 2.0f * b + c;
    const float offset = std::abs (denom) > 1.0e-9f ? 0.5f * (a - c) / denom : 0.0f;
    return (float) (sr / ((double) tau + juce::jlimit (-1.0f, 1.0f, offset)));
}

void TunerOverlay::timerCallback()
{
    const int got = feed.read (scratch.data(), (int) scratch.size());
    if (got > 0)
    {
        const int n = juce::jmin (got, (int) history.size());
        std::memmove (history.data(), history.data() + n, sizeof (float) * (history.size() - (size_t) n));
        std::copy (scratch.begin() + (got - n), scratch.begin() + got, history.end() - n);
        historyFill = juce::jmin ((int) history.size(), historyFill + n);
    }

    const float f = detectPitch();
    if (f > 20.0f)
    {
        silentFrames = 0;
        frequency = f;
        const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
        const int nearest = juce::roundToInt (midi);
        const float cents = (midi - (float) nearest) * 100.0f;
        if (nearest != note)
        {
            note = nearest;
            smoothedCents = cents;
        }
        else
        {
            smoothedCents += 0.35f * (cents - smoothedCents);
        }
    }
    else if (++silentFrames > 12)
    {
        note = -1;
    }

    if (note >= 0)
        strobePhase = std::fmod (strobePhase + smoothedCents * 0.09f + 1000.0f, 28.0f);
    repaint();
}

void TunerOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.86f));

    const auto panel = getLocalBounds().toFloat().withSizeKeepingCentre (660.0f, 400.0f);
    juce::DropShadow (juce::Colours::black, 40, { 0, 16 }).drawForRectangle (g, panel.toNearestInt());
    g.setGradientFill (juce::ColourGradient (theme.panelTop, panel.getX(), panel.getY(),
                                             theme.panelBottom, panel.getX(), panel.getBottom(), false));
    g.fillRoundedRectangle (panel, 14.0f);
    g.setColour (theme.outline);
    g.drawRoundedRectangle (panel.reduced (0.5f), 14.0f, 1.0f);

    auto inner = panel.reduced (28.0f, 20.0f);
    auto top = inner.removeFromTop (28.0f);
    g.setFont (theme.labelFont (13.0f));
    g.setColour (theme.dim);
    g.drawText ("TUNER", top, juce::Justification::centredLeft, false);
    g.setFont (theme.readoutFont (12.0f));
    g.drawText ("A4 = 440 Hz", top.withTrimmedRight (48.0f), juce::Justification::centredRight, false);

    const bool hasNote = note >= 0;
    const bool inTune = hasNote && std::abs (smoothedCents) < 3.0f;
    const auto accent = inTune ? theme.inTune : theme.text;
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    auto noteArea = inner.removeFromTop (150.0f);
    if (hasNote)
    {
        const juce::String name (names[note % 12]);
        const int octave = note / 12 - 1;
        const auto nf = theme.noteFont (150.0f);
        const float nw = juce::GlyphArrangement::getStringWidth (nf, name);
        g.setFont (nf);
        g.setColour (accent);
        g.drawText (name, noteArea.withSizeKeepingCentre (nw + 4.0f, noteArea.getHeight()), juce::Justification::centred, false);
        g.setFont (theme.noteFont (40.0f));
        g.setColour (accent.withAlpha (0.6f));
        g.drawText (juce::String (octave), noteArea.withSizeKeepingCentre (nw + 4.0f, noteArea.getHeight()).translated (nw * 0.5f + 22.0f, 34.0f),
                    juce::Justification::centred, false);
    }
    else
    {
        g.setFont (theme.noteFont (110.0f));
        g.setColour (theme.faint);
        g.drawText (juce::String::charToString (0x2014), noteArea, juce::Justification::centred, false);
    }

    auto readout = inner.removeFromTop (26.0f);
    g.setFont (theme.readoutFont (14.0f));
    g.setColour (theme.dim);
    if (hasNote)
    {
        juce::String cents = (smoothedCents >= 0.0f ? "+" : juce::String::charToString (0x2212))
                             + juce::String (std::abs (smoothedCents), 1) + " ct";
        g.drawText (cents + "    " + juce::String (frequency, 2) + " Hz", readout, juce::Justification::centred, false);
    }
    else
    {
        g.drawText ("PLAY A STRING", readout, juce::Justification::centred, false);
    }

    // strobe band
    inner.removeFromTop (14.0f);
    auto strobe = inner.removeFromTop (46.0f).reduced (20.0f, 0.0f);
    draw::displayGlass (g, strobe, 6.0f);
    {
        const juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (strobe.reduced (3.0f).toNearestInt());
        const float period = 28.0f;
        g.setColour (hasNote ? accent.withAlpha (inTune ? 0.95f : 0.7f) : theme.faint.withAlpha (0.4f));
        for (float x0 = strobe.getX() - period + strobePhase; x0 < strobe.getRight(); x0 += period)
            g.fillRect (juce::Rectangle<float> (x0, strobe.getY() + 6.0f, period * 0.5f, strobe.getHeight() - 12.0f));
        juce::ColourGradient fade (juce::Colour (0xff060606), strobe.getX(), 0.0f, juce::Colour (0x00060606), strobe.getX() + 70.0f, 0.0f, false);
        g.setGradientFill (fade);
        g.fillRect (strobe.withWidth (70.0f));
        juce::ColourGradient fadeR (juce::Colour (0x00060606), strobe.getRight() - 70.0f, 0.0f, juce::Colour (0xff060606), strobe.getRight(), 0.0f, false);
        g.setGradientFill (fadeR);
        g.fillRect (strobe.withTrimmedLeft (strobe.getWidth() - 70.0f));
    }

    // cents scale with marker
    inner.removeFromTop (12.0f);
    auto scale = inner.removeFromTop (30.0f).reduced (20.0f, 0.0f);
    for (int c = -50; c <= 50; c += 5)
    {
        const float x = scale.getX() + scale.getWidth() * ((float) c + 50.0f) / 100.0f;
        const bool major = c % 25 == 0;
        g.setColour (c == 0 ? theme.inTune : theme.dim.withAlpha (major ? 0.9f : 0.5f));
        g.fillRect (x - 0.5f, scale.getY(), c == 0 ? 2.0f : 1.0f, major ? 12.0f : 7.0f);
    }
    if (hasNote)
    {
        const float x = scale.getX() + scale.getWidth() * (juce::jlimit (-50.0f, 50.0f, smoothedCents) + 50.0f) / 100.0f;
        juce::Path marker;
        marker.addTriangle (x - 7.0f, scale.getBottom(), x + 7.0f, scale.getBottom(), x, scale.getY() + 14.0f);
        g.setColour (accent);
        g.fillPath (marker);
    }
}

} // namespace apex::ui
