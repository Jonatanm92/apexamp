#include "ThallbyssalEditor.h"
#include "apex/ui/Theme.h"

#include <cmath>

using namespace apex::ui;
namespace col = apex::ui::abyss::colours;
namespace fnt = apex::ui::abyss::fonts;
using abyss::Icon;

namespace
{
    // ---- layout (design units, 1200 x 900) --------------------------------------
    const juce::Rectangle<float> titleArea   { 36.0f, 22.0f, 384.0f, 128.0f };
    const juce::Rectangle<float> presetPlate { 440.0f, 46.0f, 320.0f, 80.0f };
    const juce::Rectangle<float> meterPanel  { 990.0f, 18.0f, 188.0f, 178.0f };
    const juce::Rectangle<float> leftPanel   { 28.0f, 210.0f, 300.0f, 456.0f };
    const juce::Rectangle<float> centrePanel { 342.0f, 150.0f, 516.0f, 516.0f };
    const juce::Rectangle<float> rightPanel  { 872.0f, 210.0f, 300.0f, 456.0f };
    const juce::Rectangle<float> chainRow    { 28.0f, 680.0f, 1144.0f, 90.0f };
    const juce::Rectangle<float> statusRow   { 28.0f, 780.0f, 1144.0f, 104.0f };
    const juce::Point<float> portalCentre    { 1022.0f, 342.0f };
    const float archHeight = 96.0f;

    juce::Rectangle<float> square (juce::Point<float> c, float d) { return juce::Rectangle<float> (d, d).withCentre (c); }
    juce::String minus (juce::String s) { return s.replace ("-", juce::String::charToString (0x2212)); }
    juce::String arrow() { return juce::String::charToString (0x2192); }

    juce::String noteName (int semitone)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[((semitone % 12) + 12) % 12];
    }

    juce::Path titlePlate (juce::Point<float> centre, float width)
    {
        return abyss::lensPlate (juce::Rectangle<float> (width, 30.0f).withCentre (centre));
    }

    float toDb (float lin) { return juce::Decibels::gainToDecibels (lin, -100.0f); }
}

//==============================================================================
/** A module panel in the centre arch: a title, knob labels, and its controls. */
struct ThallbyssalEditor::Panel : public juce::Component
{
    explicit Panel (juce::String t) : title (std::move (t)) { setInterceptsMouseClicks (false, true); }

    void paint (juce::Graphics& g) override
    {
        abyss::glowText (g, title, fnt::serif (23.0f, 0.5f), { centrePanel.getX(), 196.0f, centrePanel.getWidth(), 30.0f },
                         juce::Justification::centred, col::bone.withAlpha (0.92f), 0.25f);
        for (const auto& [area, text] : labels)
            abyss::glowText (g, text, fnt::label (15.0f, 0.1f), area, juce::Justification::centred, col::bone.withAlpha (0.85f), 0.0f);
        if (extra)
            extra (g);
    }

    juce::String title;
    std::vector<std::pair<juce::Rectangle<float>, juce::String>> labels;
    std::function<void (juce::Graphics&)> extra;
};

/** Recessed, clickable text field (preset name, IR name, target zone). */
struct ThallbyssalEditor::TextField : public juce::Component,
                                       public juce::SettableTooltipClient
{
    TextField (bool serifFont, Icon leading, bool hasLeading)
        : serif (serifFont), icon (leading), withIcon (hasLeading)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setText (const juce::String& t, bool isModified = false)
    {
        if (t != text || isModified != modified) { text = t; modified = isModified; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        if (boxed)
        {
            g.setColour (juce::Colour (0xff060504).withAlpha (0.85f));
            g.fillRect (b);
            g.setColour (isMouseOver() ? col::rimLight : col::rim);
            g.drawRect (b, 1.0f);
        }
        const auto ink = isMouseOver() ? col::emberHot : col::bone;
        auto content = b.reduced (10.0f, 0.0f);
        if (withIcon)
            abyss::drawIcon (g, icon, content.removeFromLeft (b.getHeight() * 0.62f).withSizeKeepingCentre (b.getHeight() * 0.62f, b.getHeight() * 0.62f),
                             col::ember, 0.5f);
        if (dropdown)
            abyss::drawIcon (g, Icon::target, content.removeFromRight (b.getHeight() * 0.5f).withSizeKeepingCentre (b.getHeight() * 0.45f, b.getHeight() * 0.45f),
                             col::ash, 0.0f);
        const auto font = serif ? fnt::serif (juce::jmin (18.0f, b.getHeight() * 0.52f), 0.08f) : fnt::label (juce::jmin (16.0f, b.getHeight() * 0.55f), 0.1f);
        abyss::glowText (g, text + (modified ? " *" : ""), font, content.reduced (withIcon ? 8.0f : 0.0f, 0.0f),
                         withIcon ? juce::Justification::centredLeft : juce::Justification::centred, ink, isMouseOver() ? 0.5f : 0.15f);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()) && onClick)
            onClick();
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    std::function<void()> onClick;
    bool boxed = true, dropdown = false;

private:
    bool serif;
    Icon icon;
    bool withIcon;
    juce::String text;
    bool modified = false;
};

/** A cell of the status bar: title, value, subtitle; visuals are siblings. */
struct ThallbyssalEditor::StatusCell : public juce::Component,
                                        public juce::SettableTooltipClient
{
    explicit StatusCell (bool isCaption = false) : caption (isCaption) { setInterceptsMouseClicks (false, false); }

    void set (const juce::String& t, const juce::String& v, const juce::String& s, bool isHot = false)
    {
        if (t == title && v == value && s == sub && isHot == hot)
            return;
        title = t; value = v; sub = s; hot = isHot;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        if (caption)
        {
            abyss::glowText (g, title, fnt::label (13.0f, 0.14f), b.removeFromTop (b.getHeight() * 0.5f), juce::Justification::centred, col::bone.withAlpha (0.85f), 0.0f);
            abyss::glowText (g, value, fnt::value (13.0f), b, juce::Justification::centred, col::ember, 0.3f);
            return;
        }
        const auto top = b.removeFromTop (22.0f);
        const auto ink = hot ? col::emberHot : col::bone;
        abyss::glowText (g, title, fnt::label (15.0f, 0.12f), top, juce::Justification::centredLeft, hot ? col::ember : col::bone.withAlpha (0.85f), hot ? 0.6f : 0.0f);
        abyss::glowText (g, value, fnt::value (17.0f), top, juce::Justification::centredRight, ink, hot ? 0.8f : 0.25f);
        abyss::glowText (g, sub, fnt::label (12.0f, 0.14f), b.removeFromBottom (18.0f), juce::Justification::centredLeft, col::ash, 0.0f);
    }

    juce::String title, value, sub;
    bool hot = false, caption;
};

/** A file you can drag out of the plugin into the DAW (or click to reveal):
    the last riff's DI, bass or kick MIDI. */
struct ThallbyssalEditor::DragTile : public juce::Component,
                                     public juce::SettableTooltipClient
{
    DragTile (juce::String n, juce::String s, Icon i) : name (std::move (n)), sub (std::move (s)), icon (i)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    std::function<juce::File()> makeFile;

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        const bool over = isMouseOver();
        g.setColour (juce::Colour (0xff070504).withAlpha (0.9f));
        g.fillRoundedRectangle (b, 3.0f);
        juce::Path border;
        border.addRoundedRectangle (b, 3.0f);
        float dash[] = { 4.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, border, dash, 2);
        if (over || flash > 0.0f)
            abyss::glowStroke (g, border, 1.0f, col::ember, juce::jmax (0.5f, flash));
        else
        {
            g.setColour (col::rimLight);
            g.fillPath (dashed);
        }
        const auto iconArea = b.removeFromLeft (b.getHeight()).reduced (10.0f);
        abyss::drawIcon (g, icon, iconArea, over ? col::emberHot : col::ember, over ? 0.6f : 0.25f);
        auto text = b.reduced (2.0f, 6.0f);
        abyss::glowText (g, name, fnt::labelBold (15.0f, 0.12f), text.removeFromTop (text.getHeight() * 0.55f), juce::Justification::centredLeft,
                         over ? col::emberHot : col::bone, over ? 0.4f : 0.0f);
        abyss::glowText (g, status.isNotEmpty() ? status : sub, fnt::label (11.0f, 0.12f), text, juce::Justification::centredLeft, col::ash, 0.0f);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 5 || ! makeFile)
            return;
        dragging = true;
        const auto file = makeFile();
        if (file.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! dragging && getLocalBounds().contains (e.getPosition()) && makeFile)
        {
            const auto file = makeFile();
            if (file.existsAsFile())
                file.revealToUser();
        }
        dragging = false;
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    void setStatus (const juce::String& s) { if (s != status) { status = s; repaint(); } }
    void pulse() { flash = 1.0f; repaint(); }
    void tick() { if (flash > 0.0f) { flash = juce::jmax (0.0f, flash - 0.05f); repaint(); } }

private:
    juce::String name, sub, status;
    Icon icon;
    bool dragging = false;
    float flash = 0.0f;
};

/** "MATCH STATUS" line: icon + verdict. */
struct ThallbyssalEditor::MatchStatus : public juce::Component
{
    MatchStatus() { setInterceptsMouseClicks (false, false); }

    void set (const juce::String& t, bool good, float pulse)
    {
        if (t == text && good == ok && std::abs (pulse - glow) < 0.02f)
            return;
        text = t; ok = good; glow = pulse;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const auto iconArea = b.removeFromLeft (b.getHeight()).reduced (4.0f);
        abyss::drawIcon (g, ok ? Icon::check : Icon::warning, iconArea, col::ember, 0.5f + 0.5f * glow);
        b.removeFromLeft (8.0f);
        abyss::glowText (g, text, fnt::label (16.0f, 0.1f), b, juce::Justification::centredLeft, ok ? col::emberHot : col::ember, 0.4f + 0.4f * glow);
    }

    juce::String text;
    bool ok = false;
    float glow = 0.0f;
};

//==============================================================================
ThallbyssalEditor::ThallbyssalEditor (ApexAmpProcessor& p)
    : EditorBase (p, p.apvts, p.presets, "Amp", designWidth, designHeight, &p.tunerFeed, false),
      proc (p)
{
    setThemeLookAndFeel (&abyssLookAndFeel);
    setTunerTheme (abyss::tunerTheme());

    // ---- the static art --------------------------------------------------------
    abyss::BackdropSpec spec;
    spec.designWidth = designWidth;
    spec.designHeight = designHeight;
    spec.seed = 7;
    spec.recesses = { abyss::archPanel (centrePanel.reduced (6.0f), archHeight - 6.0f, 18.0f) };
    for (auto r : { leftPanel, rightPanel, meterPanel, chainRow, statusRow })
    {
        juce::Path p;
        p.addRoundedRectangle (r.reduced (5.0f), 14.0f);
        spec.recesses.push_back (p);
    }
    spec.hotspots = { { { 200.0f, 80.0f }, 150.0f }, { portalCentre, 120.0f }, { { 600.0f, 832.0f }, 90.0f },
                      { { 20.0f, 700.0f }, 70.0f }, { { 1180.0f, 690.0f }, 70.0f }, { { 1060.0f, 200.0f }, 60.0f },
                      { { 600.0f, 660.0f }, 60.0f } };
    spec.ornaments = [] (juce::Graphics& art, juce::Graphics& glow)
    {
        // sigil behind the title
        abyss::sigil (art, { 226.0f, 76.0f }, 74.0f, col::rimLight, 0.35f, 3);
        abyss::sigil (glow, { 226.0f, 76.0f }, 74.0f, col::emberDeep, 0.25f, 3);

        // title: condensed Cinzel Decorative, molten fill, cracks, glowing edges
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (fnt::title (100.0f), "THALLBYSSAL", 0.0f, 0.0f);
            juce::Path glyphs;
            ga.createPath (glyphs);
            glyphs.applyTransform (glyphs.getTransformToScaleToFit (titleArea.withTrimmedBottom (34.0f).withTrimmedTop (6.0f), false));

            glow.setColour (col::ember.withAlpha (0.55f));
            glow.fillPath (glyphs);
            glow.setColour (col::emberHot.withAlpha (0.6f));
            glow.strokePath (glyphs, juce::PathStrokeType (2.2f));

            const auto gb = glyphs.getBounds();
            art.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a3a16), gb.getX(), gb.getY(),
                                                       juce::Colour (0xff2a0d05), gb.getX(), gb.getBottom(), false));
            art.fillPath (glyphs);
            {
                const juce::Graphics::ScopedSaveState s (art);
                art.reduceClipRegion (glyphs);
                std::uint32_t st = 99u;
                auto r01 = [&st] { st = st * 1664525u + 1013904223u; return (float) (st >> 8) / 16777216.0f; };
                for (int i = 0; i < 38; ++i)
                {
                    juce::Path crack;
                    float x = gb.getX() + gb.getWidth() * r01(), y = gb.getY() + gb.getHeight() * r01();
                    crack.startNewSubPath (x, y);
                    for (int k = 0; k < 5; ++k)
                    {
                        x += (r01() - 0.5f) * 14.0f;
                        y += (r01() - 0.3f) * 12.0f;
                        crack.lineTo (x, y);
                    }
                    art.setColour (col::emberHot.withAlpha (0.45f + 0.4f * r01()));
                    art.strokePath (crack, juce::PathStrokeType (0.5f + 0.6f * r01()));
                }
                art.setColour (col::emberHot.withAlpha (0.55f));
                art.strokePath (glyphs, juce::PathStrokeType (2.6f));
            }
            art.setColour (juce::Colour (0xff0a0504));
            art.strokePath (glyphs, juce::PathStrokeType (0.9f));
        }
        art.setColour (juce::Colour (0xff070504).withAlpha (0.75f));
        art.fillRect (juce::Rectangle<float> (titleArea.getX() + 30.0f, titleArea.getBottom() - 22.0f, titleArea.getWidth() - 60.0f, 18.0f));
        abyss::glowText (art, "DEEPER, HEAVIER, BEYOND MEASURE.", fnt::serifLight (12.0f, 0.42f),
                         { titleArea.getX(), titleArea.getBottom() - 22.0f, titleArea.getWidth(), 18.0f },
                         juce::Justification::centred, col::ember, 0.35f);

        // preset plate
        abyss::ornateFrame (art, glow, abyss::lensPlate (presetPlate), 11, 0.5f);
        abyss::glowText (art, "PRESET", fnt::label (11.0f, 0.3f), { presetPlate.getX(), presetPlate.getY() + 6.0f, presetPlate.getWidth(), 14.0f },
                         juce::Justification::centred, col::ash, 0.0f);

        // meters
        {
            juce::Path p;
            p.addRoundedRectangle (meterPanel, 14.0f);
            abyss::ornateFrame (art, glow, p, 21, 0.6f, 0.8f);
            for (auto [x, t] : { std::pair<float, const char*> { 1021.0f, "INPUT" }, { 1069.0f, "OUTPUT" } })
                abyss::glowText (art, t, fnt::label (12.0f, 0.12f), { x - 30.0f, 26.0f, 60.0f, 14.0f }, juce::Justification::centred, col::ash, 0.0f);
        }

        // side panels with title plates
        for (auto [r, title, seed] : { std::tuple<juce::Rectangle<float>, const char*, int> { leftPanel, "INPUT MATCH", 31 },
                                       { rightPanel, "CAB CHAMBER", 41 } })
        {
            juce::Path p;
            p.addRoundedRectangle (r, 16.0f);
            abyss::ornateFrame (art, glow, p, seed, 0.5f);
            const auto plate = titlePlate ({ r.getCentreX(), r.getY() }, 214.0f);
            art.setColour (juce::Colour (0xff0c0908));
            art.fillPath (plate);
            abyss::ornateFrame (art, glow, plate, seed + 1, 0.4f, 0.7f);
            abyss::glowText (art, title, fnt::serif (15.0f, 0.42f), plate.getBounds(), juce::Justification::centred, col::bone, 0.15f);
        }

        // input match furniture
        abyss::glowText (art, "DI LEVEL", fnt::label (12.0f, 0.14f), { 44.0f, 236.0f, 80.0f, 14.0f }, juce::Justification::centredLeft, col::ash, 0.0f);
        for (float y : { 482.0f, 546.0f })
        {
            art.setColour (juce::Colour (0xff070504).withAlpha (0.7f));
            art.fillRect (juce::Rectangle<float> (44.0f, y, 268.0f, 58.0f));
            art.setColour (col::rim);
            art.drawRect (juce::Rectangle<float> (44.0f, y, 268.0f, 58.0f), 1.0f);
        }
        abyss::glowText (art, "MATCH STATUS", fnt::label (12.0f, 0.14f), { 54.0f, 486.0f, 200.0f, 14.0f }, juce::Justification::centredLeft, col::ash, 0.0f);
        abyss::glowText (art, "TARGET ZONE", fnt::label (12.0f, 0.14f), { 54.0f, 550.0f, 200.0f, 14.0f }, juce::Justification::centredLeft, col::ash, 0.0f);

        // centre arch, finials
        {
            const auto arch = abyss::archPanel (centrePanel, archHeight, 22.0f);
            abyss::ornateFrame (art, glow, arch, 51, 0.6f, 1.1f);
            juce::Path inner = abyss::archPanel (centrePanel.reduced (10.0f), archHeight - 10.0f, 16.0f);
            art.setColour (col::rim.withAlpha (0.6f));
            art.strokePath (inner, juce::PathStrokeType (0.8f));

            const juce::Point<float> top (centrePanel.getCentreX(), centrePanel.getY() + 8.0f);
            art.setColour (juce::Colour (0xff0b0807));
            art.fillEllipse (square (top, 40.0f));
            abyss::drawIcon (glow, Icon::trident, square (top, 30.0f), col::ember, 0.6f);
            abyss::drawIcon (art, Icon::trident, square (top, 30.0f), col::emberHot, 0.0f);
            art.setColour (col::rimLight.withAlpha (0.6f));
            art.drawEllipse (square (top, 40.0f), 1.0f);

            const juce::Point<float> bottom (centrePanel.getCentreX(), centrePanel.getBottom());
            juce::Path dagger;
            dagger.startNewSubPath (bottom.x, bottom.y - 22.0f); dagger.lineTo (bottom.x, bottom.y + 12.0f);
            dagger.startNewSubPath (bottom.x - 8.0f, bottom.y - 12.0f); dagger.lineTo (bottom.x + 8.0f, bottom.y - 12.0f);
            glow.setColour (col::ember.withAlpha (0.7f));
            glow.strokePath (dagger, juce::PathStrokeType (2.0f));
            art.setColour (col::emberHot);
            art.strokePath (dagger, juce::PathStrokeType (1.0f));
        }

        // cab chamber: the rune ring around the portal
        {
            const float R = 112.0f;
            art.setColour (col::rim);
            art.drawEllipse (square (portalCentre, R * 2.0f), 1.2f);
            art.drawEllipse (square (portalCentre, R * 2.0f - 26.0f), 0.8f);
            for (int i = 0; i < 28; ++i)
            {
                const float a = (float) i / 28.0f * juce::MathConstants<float>::twoPi;
                const float rr = R - 6.5f;
                auto rune = abyss::runePath (100 + i, { -4.0f, -6.0f, 8.0f, 12.0f });
                rune.applyTransform (juce::AffineTransform::rotation (a + juce::MathConstants<float>::halfPi)
                                         .translated (portalCentre + juce::Point<float> (std::cos (a), std::sin (a)) * rr));
                art.setColour (col::ash.withAlpha (0.8f));
                art.strokePath (rune, juce::PathStrokeType (1.0f));
                glow.setColour (col::emberDeep.withAlpha (0.5f));
                glow.strokePath (rune, juce::PathStrokeType (1.4f));
            }
            for (int i = 0; i < 4; ++i)
            {
                const float a = (float) i * juce::MathConstants<float>::halfPi;
                const auto pt = portalCentre + juce::Point<float> (std::cos (a), std::sin (a)) * (R + 6.0f);
                art.setColour (col::rimLight);
                art.fillEllipse (square (pt, 5.0f));
            }
            abyss::glowText (art, "IR SLOT", fnt::label (12.0f, 0.14f), { rightPanel.getX(), 462.0f, rightPanel.getWidth(), 14.0f },
                             juce::Justification::centred, col::ash, 0.0f);
        }

        // chain row and status bar
        for (auto [r, seed] : { std::pair<juce::Rectangle<float>, int> { chainRow, 61 }, { statusRow, 71 } })
        {
            juce::Path p;
            p.addRoundedRectangle (r, 18.0f);
            abyss::ornateFrame (art, glow, p, seed, 0.5f);
        }
        for (float x : { 276.0f, 542.0f, 658.0f, 870.0f, 1052.0f })
        {
            art.setColour (col::rim.withAlpha (0.8f));
            art.drawLine (x, statusRow.getY() + 14.0f, x, statusRow.getBottom() - 14.0f, 1.0f);
        }
    };
    backdrop = std::make_unique<abyss::Backdrop> (spec);
    stage.addAndMakeVisible (*backdrop);
    backdrop->setBounds (stage.getLocalBounds());

    buildHeader();
    buildInputMatch();
    buildModules();
    buildCabChamber();
    buildChain();
    buildStatus();

    selectModule ((int) apvts.state.getProperty ("abyssModule", (int) amp));
    tick();
    if (proc.licensing->getStatus (proc.licenceProduct).expired)
        showUnlock();
}

ThallbyssalEditor::~ThallbyssalEditor()
{
    setThemeLookAndFeel (nullptr);
}

//==============================================================================
float ThallbyssalEditor::param (const char* id) const
{
    return apvts.getRawParameterValue (id)->load();
}

void ThallbyssalEditor::setParam (const juce::String& paramId, float value, const juce::String& undoName)
{
    auto* p = apvts.getParameter (paramId);
    if (undoName.isNotEmpty())
        proc.undoManager.beginNewTransaction (undoName);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (value));
    p->endChangeGesture();
}

void ThallbyssalEditor::stepParam (const juce::String& paramId, int delta, int minValue, int maxValue)
{
    auto* p = apvts.getParameter (paramId);
    const int current = juce::roundToInt (p->convertFrom0to1 (p->getValue()));
    const int next = juce::jlimit (minValue, maxValue, current + delta);
    if (next != current)
        setParam (paramId, (float) next, p->getName (32));
}

abyss::Knob& ThallbyssalEditor::addKnob (juce::Component& parent, const juce::String& paramId, const juce::String& label,
                                         juce::Point<float> centre, float d, const juce::String& tip)
{
    auto knob = std::make_unique<abyss::Knob>();
    auto& k = *knob;
    parent.addAndMakeVisible (k);
    const float side = d * abyss::Knob::boundsRatio;
    if (label.isNotEmpty())
    {
        // the caption above the knob shows the value while it is touched
        k.setCaption (label);
        const float w = juce::jmax (side, 118.0f);
        k.setBounds (juce::Rectangle<float> (centre.x - w * 0.5f, centre.y - side * 0.5f - abyss::Knob::captionHeight,
                                             w, side + abyss::Knob::captionHeight).toNearestInt());
    }
    else
    {
        k.setBounds (square (centre, side).toNearestInt());
        k.setPopupParent (&stage);
    }
    k.setTooltip (tip);
    auto* p = apvts.getParameter (paramId);
    sliderAttachments.push_back (abyss::attach (k, *p, &proc.undoManager));
    if (auto* ranged = dynamic_cast<juce::AudioParameterFloat*> (p))
        if (ranged->range.start < 0.0f && ranged->range.end > 0.0f)
            k.setLitOrigin (0.0);
    owned.push_back (std::move (knob));
    return k;
}

abyss::GlowButton& ThallbyssalEditor::addToggle (juce::Component& parent, const juce::String& paramId, const juce::String& text,
                                                 juce::Rectangle<float> bounds, const juce::String& tip)
{
    auto button = std::make_unique<abyss::GlowButton> (text, abyss::GlowButton::Style::plain);
    auto& b = *button;
    b.setClickingTogglesState (true);
    b.setTooltip (tip);
    parent.addAndMakeVisible (b);
    b.setBounds (bounds.toNearestInt());
    buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*apvts.getParameter (paramId), b, &proc.undoManager));
    owned.push_back (std::move (button));
    return b;
}

//==============================================================================
void ThallbyssalEditor::buildHeader()
{
    auto& host = *backdrop;

    auto prev = std::make_unique<abyss::GlowButton> (Icon::previous, juce::String(), abyss::GlowButton::Style::text);
    auto next = std::make_unique<abyss::GlowButton> (Icon::next, juce::String(), abyss::GlowButton::Style::text);
    prev->setBounds (juce::Rectangle<float> (30.0f, 34.0f).withCentre ({ presetPlate.getX() + 46.0f, presetPlate.getCentreY() + 7.0f }).toNearestInt());
    next->setBounds (juce::Rectangle<float> (30.0f, 34.0f).withCentre ({ presetPlate.getRight() - 46.0f, presetPlate.getCentreY() + 7.0f }).toNearestInt());
    prev->setTooltip ("Previous preset");
    next->setTooltip ("Next preset");
    prev->onClick = [this] { presets.loadPrevious(); };
    next->onClick = [this] { presets.loadNext(); };
    host.addAndMakeVisible (*prev);
    host.addAndMakeVisible (*next);

    auto name = std::make_unique<TextField> (true, Icon::check, false);
    name->boxed = true;
    name->setBounds (juce::Rectangle<float> (presetPlate.getWidth() - 132.0f, 36.0f).withCentre ({ presetPlate.getCentreX(), presetPlate.getCentreY() + 7.0f }).toNearestInt());
    name->onClick = [this] { showPresetMenu (*presetName); };
    host.addAndMakeVisible (*name);
    presetName = name.get();
    owned.push_back (std::move (prev));
    owned.push_back (std::move (next));
    owned.push_back (std::move (name));

    auto make = [&] (std::unique_ptr<abyss::GlowButton> b, juce::Rectangle<float> r, const juce::String& tip)
    {
        auto* raw = b.get();
        raw->setBounds (r.toNearestInt());
        raw->setTooltip (tip);
        host.addAndMakeVisible (*raw);
        owned.push_back (std::move (b));
        return raw;
    };
    const float y = 70.0f, h = 34.0f;
    saveButton = make (std::make_unique<abyss::GlowButton> ("SAVE", abyss::GlowButton::Style::tab), { 790.0f, y, 56.0f, h }, "Save the current sound as a preset");
    aButton    = make (std::make_unique<abyss::GlowButton> ("A", abyss::GlowButton::Style::tab), { 852.0f, y, 30.0f, h }, "Compare: setting A");
    bButton    = make (std::make_unique<abyss::GlowButton> ("B", abyss::GlowButton::Style::tab), { 882.0f, y, 30.0f, h }, "Compare: setting B");
    undoButton = make (std::make_unique<abyss::GlowButton> (Icon::undo), { 918.0f, y, 32.0f, h }, "Undo");
    redoButton = make (std::make_unique<abyss::GlowButton> (Icon::redo), { 952.0f, y, 32.0f, h }, "Redo");
    saveButton->onClick = [this] { showSaveDialog(); };
    aButton->onClick = [this] { presets.selectSlot (0); };
    bButton->onClick = [this] { presets.selectSlot (1); };
    undoButton->onClick = [this] { proc.undoManager.undo(); };
    redoButton->onClick = [this] { proc.undoManager.redo(); };

    tunerButton    = make (std::make_unique<abyss::GlowButton> (Icon::tuner, "TUNER"), { 1124.0f, 36.0f, 46.0f, 66.0f }, "Strobe tuner");
    settingsButton = make (std::make_unique<abyss::GlowButton> (Icon::settings, "SETUP"), { 1124.0f, 112.0f, 46.0f, 66.0f }, "Settings");
    tunerButton->onClick = [this] { toggleTuner(); tunerButton->setToggleState (isTunerOpen(), juce::dontSendNotification); };
    settingsButton->onClick = [this] { showSettingsMenu (*settingsButton); };
    onTunerClosed = [this] { tunerButton->setToggleState (false, juce::dontSendNotification); };

    trialBadge = make (std::make_unique<abyss::GlowButton> ("TRIAL", abyss::GlowButton::Style::tab), { 790.0f, 112.0f, 194.0f, 30.0f },
                       "Trial and licence");
    trialBadge->onClick = [this] { showUnlock(); };

    auto in  = std::make_unique<abyss::VMeter> (-60.0f, 0.0f, std::vector<float> {}, true);
    auto out = std::make_unique<abyss::VMeter> (-60.0f, 0.0f, std::vector<float> { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f }, true);
    in->setBounds (998, 42, 46, 146);
    out->setBounds (1046, 42, 74, 146);
    host.addAndMakeVisible (*in);
    host.addAndMakeVisible (*out);
    inMeter = in.get();
    outMeter = out.get();
    owned.push_back (std::move (in));
    owned.push_back (std::move (out));
}

void ThallbyssalEditor::buildInputMatch()
{
    auto& host = *backdrop;

    auto di = std::make_unique<abyss::VMeter> (-60.0f, 12.0f, std::vector<float> { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -60.0f }, false);
    di->setTargetMark (true);
    di->setBounds (40, 254, 64, 212);
    host.addAndMakeVisible (*di);
    diMeter = di.get();
    owned.push_back (std::move (di));

    auto r = std::make_unique<abyss::Radar>();
    r->setBounds (110, 252, 212, 212);
    r->setTooltip ("Calibration radar: the closer to the centre, the closer your DI is to the target level");
    host.addAndMakeVisible (*r);
    radar = r.get();
    owned.push_back (std::move (r));

    auto status = std::make_unique<MatchStatus>();
    status->setBounds (52, 504, 254, 32);
    host.addAndMakeVisible (*status);
    matchStatus = status.get();
    owned.push_back (std::move (status));

    auto zone = std::make_unique<TextField> (false, Icon::sun, true);
    zone->boxed = false;
    zone->dropdown = true;
    zone->setBounds (48, 566, 260, 34);
    zone->setTooltip ("Target zone: how hard Auto Input drives the amp");
    zone->onClick = [this] { showTargetMenu(); };
    host.addAndMakeVisible (*zone);
    targetZone = zone.get();
    owned.push_back (std::move (zone));

    auto cal = std::make_unique<abyss::GlowButton> ("CALIBRATE DI", abyss::GlowButton::Style::plain);
    cal->setBounds (66, 614, 224, 42);
    cal->setTooltip ("Auto Input: play your heaviest riff for a few seconds; the input level is set for you");
    cal->setPulsing (true);
    cal->onClick = [this] { if (! proc.isAutoInputListening()) proc.startAutoInput(); };
    host.addAndMakeVisible (*cal);
    calibrate = cal.get();
    owned.push_back (std::move (cal));
}

//==============================================================================
void ThallbyssalEditor::buildModules()
{
    auto& host = *backdrop;
    auto makePanel = [&] (int index, const juce::String& title)
    {
        auto panel = std::make_unique<Panel> (title);
        panel->setBounds (host.getLocalBounds());
        host.addChildComponent (*panel);
        panels[(size_t) index] = panel.get();
        owned.push_back (std::move (panel));
        return panels[(size_t) index];
    };
    const float cx = centrePanel.getCentreX();

    // ---- AMP: the abyssal core -------------------------------------------------------
    {
        auto& p = *makePanel (amp, "ABYSSAL CORE");
        const float xs[] = { 420.0f, 540.0f, 660.0f, 780.0f };
        const float d = 62.0f, row1 = 306.0f, row2 = 436.0f;
        addKnob (p, "inputGain", "GAIN", { xs[0], row1 }, d, "Gain: drive into the amp");
        addKnob (p, "bass", "BASS", { xs[1], row1 }, d, "Bass shelf, 110 Hz");
        addKnob (p, "mid", "MID", { xs[2], row1 }, d, "Mid, 700 Hz");
        addKnob (p, "treble", "TREBLE", { xs[3], row1 }, d, "Treble shelf, 2.6 kHz");
        addKnob (p, "presence", "PRESENCE", { xs[0], row2 }, d, "Presence: brightness after the cab, 3.5 kHz");
        addKnob (p, "depth", "DEPTH", { xs[1], row2 }, d, "Depth: power-amp resonance around 85 Hz");
        addKnob (p, "tight", "TIGHT", { xs[2], row2 }, d, "Tight: low-cut before the amp for faster palm mutes");
        addKnob (p, "outputGain", "MASTER", { xs[3], row2 }, d, "Master output level");

        const char* names[] = { "BITE", "BODY", "EDGE", "BLEND", "USER" };
        const Icon icons[] = { Icon::link, Icon::spiral, Icon::sun, Icon::trident, Icon::load };
        const char* tips[] = { "Rig: Bite capture", "Rig: Body capture", "Rig: Edge capture",
                               "Blend all three captures, loudness matched", "Your own .nam capture" };
        for (int i = 0; i < 5; ++i)
        {
            auto b = std::make_unique<abyss::GlowButton> (Icon::check, names[i], abyss::GlowButton::Style::tab);
            b->setIcon (icons[i]);
            b->setBounds (juce::Rectangle<float> (376.0f + (float) i * 90.0f, 500.0f, 84.0f, 36.0f).toNearestInt());
            b->setTooltip (tips[i]);
            b->onClick = [this, i] { selectRig (i); };
            p.addAndMakeVisible (*b);
            rigButtons[(size_t) i] = b.get();
            owned.push_back (std::move (b));
        }

        const char* trimIds[] = { "mixBite", "mixBody", "mixEdge" };
        const char* trimNames[] = { "BITE", "BODY", "EDGE" };
        for (int i = 0; i < 3; ++i)
        {
            auto& k = addKnob (p, trimIds[i], {}, { cx - 90.0f + 90.0f * (float) i, 598.0f }, 30.0f, juce::String ("Blend level: ") + trimNames[i]);
            blendTrims[(size_t) i] = &k;
        }
        p.extra = [this, cx] (juce::Graphics& g)
        {
            const bool blend = param ("rigMode") > 0.5f;
            if (blend)
            {
                const char* trimNames[] = { "BITE", "BODY", "EDGE" };
                for (int i = 0; i < 3; ++i)
                    abyss::glowText (g, trimNames[i], fnt::label (11.0f, 0.12f), { cx - 90.0f + 90.0f * (float) i - 30.0f, 618.0f, 60.0f, 14.0f },
                                     juce::Justification::centred, col::ash, 0.0f);
            }
            else
            {
                abyss::glowText (g, "DESIGNED FOR THE LOWEST", fnt::serifLight (11.0f, 0.45f), { centrePanel.getX(), 566.0f, centrePanel.getWidth(), 16.0f },
                                 juce::Justification::centred, col::ash, 0.0f);
                abyss::glowText (g, "TUNINGS AND THE DEEPEST TONES", fnt::serifLight (11.0f, 0.45f), { centrePanel.getX(), 586.0f, centrePanel.getWidth(), 16.0f },
                                 juce::Justification::centred, col::ash, 0.0f);
            }
        };
    }

    // ---- DROP: the descent ---------------------------------------------------------
    {
        auto& p = *makePanel (drop, "THE DESCENT");
        auto down = std::make_unique<abyss::GlowButton> (Icon::previous, juce::String(), abyss::GlowButton::Style::icon);
        auto up   = std::make_unique<abyss::GlowButton> (Icon::next, juce::String(), abyss::GlowButton::Style::icon);
        down->setBounds (386, 284, 40, 56);
        up->setBounds (774, 284, 40, 56);
        down->setTooltip ("Down a semitone");
        up->setTooltip ("Up a semitone");
        down->onClick = [this] { stepParam ("dropShift", -1, -12, 12); };
        up->onClick   = [this] { stepParam ("dropShift", 1, -12, 12); };
        p.addAndMakeVisible (*down);
        p.addAndMakeVisible (*up);
        owned.push_back (std::move (down));
        owned.push_back (std::move (up));

        addKnob (p, "dropBody", "BODY", { 490.0f, 460.0f }, 70.0f, "Body: keeps the guitar's pickup and body resonances where a real drop tuning keeps them");
        addKnob (p, "dropSub", "SUB", { 710.0f, 460.0f }, 70.0f, "Sub: an octave-down layer under the dropped guitar");
        addToggle (p, "dropOn", "ENGAGE", { cx - 80.0f, 560.0f, 160.0f, 40.0f }, "Drop on / off");

        p.extra = [this] (juce::Graphics& g)
        {
            auto display = juce::Rectangle<float> (436.0f, 252.0f, 328.0f, 120.0f);
            g.setColour (juce::Colour (0xff040302));
            g.fillRoundedRectangle (display, 6.0f);
            g.setColour (col::rim);
            g.drawRoundedRectangle (display, 6.0f, 1.0f);
            const int st = juce::roundToInt (param ("dropShift"));
            const bool on = param ("dropOn") > 0.5f;
            const auto digits = minus (st > 0 ? "+" + juce::String (st) : juce::String (st));
            abyss::glowText (g, digits, fnt::serif (62.0f, 0.05f), display.withTrimmedBottom (34.0f).translated (0.0f, 6.0f), juce::Justification::centred,
                             on ? col::emberHot : col::ember.withAlpha (0.45f), on ? 1.0f : 0.2f);
            const auto caption = "E STANDARD  " + arrow() + "  " + noteName (4 + st) + (st <= -12 ? "  " + minus ("-1") + " OCTAVE" : " STANDARD");
            abyss::glowText (g, caption, fnt::label (15.0f, 0.16f), display.removeFromBottom (34.0f), juce::Justification::centred,
                             col::bone.withAlpha (on ? 0.9f : 0.5f), 0.0f);
            abyss::glowText (g, "LIVE PITCH ENGINE  " + juce::String::charToString (0x00B7) + "  FORMANT-CORRECT  " + juce::String::charToString (0x00B7) + "  "
                                    + juce::String (proc.getLatencyMs(), 1) + " MS",
                             fnt::label (12.0f, 0.16f), { centrePanel.getX(), 614.0f, centrePanel.getWidth(), 16.0f },
                             juce::Justification::centred, col::ash, 0.0f);
        };
    }

    // ---- GATE ---------------------------------------------------------------------
    {
        auto& p = *makePanel (gate, "THE GATE");
        addKnob (p, "gate", "THRESHOLD", { 490.0f, 360.0f }, 80.0f, "Gate threshold");
        addKnob (p, "gateHold", "HOLD", { 710.0f, 360.0f }, 80.0f, "Hold: how long the gate stays open after the last note");
        addToggle (p, "gateOn", "ENGAGE", { cx - 80.0f, 560.0f, 160.0f, 40.0f }, "Gate on / off");
        p.extra = [this, cx] (juce::Graphics& g)
        {
            const bool open = inDb > param ("gate");
            const bool on = param ("gateOn") > 0.5f;
            abyss::glowSpot (g, { cx, 494.0f }, 26.0f, col::ember, on && open ? 0.7f : 0.1f);
            g.setColour (on && open ? col::emberHot : col::emberDim);
            g.fillEllipse (square ({ cx, 494.0f }, 9.0f));
            abyss::glowText (g, on ? (open ? "OPEN" : "CLOSED") : "BYPASSED", fnt::label (13.0f, 0.2f), { cx - 80.0f, 508.0f, 160.0f, 18.0f },
                             juce::Justification::centred, col::ash, 0.0f);
        };
    }

    // ---- BOOST ----------------------------------------------------------------------
    {
        auto& p = *makePanel (boost, "THE BOOST");
        addKnob (p, "boostDrive", "DRIVE", { 450.0f, 380.0f }, 70.0f, "Boost drive");
        addKnob (p, "boostTone", "TONE", { cx, 380.0f }, 70.0f, "Boost tone");
        addKnob (p, "boostLevel", "LEVEL", { 750.0f, 380.0f }, 70.0f, "Boost level");
        addToggle (p, "boostOn", "ENGAGE", { cx - 80.0f, 560.0f, 160.0f, 40.0f }, "Boost on / off");
        p.extra = [] (juce::Graphics& g)
        {
            abyss::glowText (g, "MID-FOCUSED OVERDRIVE IN FRONT OF THE AMP", fnt::label (12.0f, 0.16f), { centrePanel.getX(), 614.0f, centrePanel.getWidth(), 16.0f },
                             juce::Justification::centred, col::ash, 0.0f);
        };
    }

    // ---- SHAPE: the chug forge ---------------------------------------------------------
    {
        auto& p = *makePanel (shape, "CHUG FORGE");
        addKnob (p, "chug", "CHUG", { 450.0f, 360.0f }, 70.0f, "Chug: punch on every pick attack, read from the DI before the amp; held notes stay untouched");
        addKnob (p, "chugFreq", "FREQUENCY", { cx, 360.0f }, 70.0f, "Where the punch sits: low for thump, high for pick click");
        addKnob (p, "dirt", "GROWL", { 750.0f, 360.0f }, 70.0f, "Growl: a parallel, envelope-following fuzz on the low end only, after the amp");
        addToggle (p, "shapeOn", "ENGAGE", { cx - 80.0f, 560.0f, 160.0f, 40.0f }, "Shape on / off");
        auto bar = std::make_unique<abyss::HBar>();
        bar->setBounds (450, 486, 300, 8);
        p.addAndMakeVisible (*bar);
        punchBar = bar.get();
        owned.push_back (std::move (bar));
        p.extra = [] (juce::Graphics& g)
        {
            abyss::glowText (g, "PUNCH", fnt::label (12.0f, 0.2f), { 450.0f, 466.0f, 300.0f, 16.0f }, juce::Justification::centred, col::ash, 0.0f);
        };
    }

    // ---- FX: the void (echo + abyss) -----------------------------------------------------
    {
        auto& p = *makePanel (fx, "THE VOID");
        const float xs[] = { 430.0f, 550.0f, 670.0f, 790.0f };
        const float d = 46.0f;
        addToggle (p, "delayOn", "ECHO", { 372.0f, 236.0f, 96.0f, 30.0f }, "Echo on / off (repeats ring out when switched off)");
        echoTimeKnob = &addKnob (p, "delayTime", "TIME", { xs[0], 330.0f }, d, {});
        sliderAttachments.pop_back();   // bound by bindEchoTime() to the time or the division
        addKnob (p, "delayFeedback", "FEEDBACK", { xs[1], 330.0f }, d, "Echo feedback: how many repeats");
        addKnob (p, "delayDuck", "DUCK", { xs[2], 330.0f }, d, "Duck: keeps the repeats down while you play, lets them bloom in the gaps");
        addKnob (p, "delayMix", "MIX", { xs[3], 330.0f }, d, "Echo level");

        auto sync = std::make_unique<abyss::GlowButton> ("SYNC", abyss::GlowButton::Style::tab);
        sync->setClickingTogglesState (true);
        sync->setBounds (726, 236, 56, 30);
        sync->setTooltip ("Sync the echo to the host tempo");
        p.addAndMakeVisible (*sync);
        buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*apvts.getParameter ("delaySync"), *sync, &proc.undoManager));
        auto tap = std::make_unique<abyss::GlowButton> ("TAP", abyss::GlowButton::Style::tab);
        tap->setBounds (786, 236, 48, 30);
        tap->setTooltip ("Tap the echo time");
        tap->onClick = [this] { tapTempo(); };
        p.addAndMakeVisible (*tap);
        owned.push_back (std::move (sync));
        owned.push_back (std::move (tap));
        syncWatcher = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter ("delaySync"), [this] (float) { bindEchoTime(); });
        bindEchoTime();

        addToggle (p, "reverbOn", "ABYSS", { 372.0f, 412.0f, 96.0f, 30.0f }, "Abyss reverb on / off (the tail rings out when switched off)");
        addKnob (p, "reverbDecay", "DECAY", { xs[0], 506.0f }, d, "Reverb decay time");
        addKnob (p, "reverbAbyss", "DEPTH", { xs[1], 506.0f }, d, "Abyss: an octave-down shimmer; the tail sinks an octave on every pass");
        addKnob (p, "reverbTone", "TONE", { xs[2], 506.0f }, d, "Reverb tone: dark to bright");
        addKnob (p, "reverbMix", "MIX", { xs[3], 506.0f }, d, "Reverb level");

        p.extra = [this] (juce::Graphics& g)
        {
            const bool synced = param ("delaySync") > 0.5f;
            auto echo = juce::String (juce::roundToInt (proc.getEchoMs())) + " MS";
            if (synced)
                echo = apvts.getParameter ("delayDiv")->getCurrentValueAsText() + "   " + echo + "   " + juce::String (juce::roundToInt (proc.getHostBpm())) + " BPM";
            abyss::glowText (g, echo, fnt::value (16.0f), { 476.0f, 236.0f, 244.0f, 30.0f }, juce::Justification::centred, col::emberHot, 0.3f);
            const float decay = param ("reverbDecay");
            abyss::glowText (g, "DECAY " + juce::String (decay, decay < 10.0f ? 1 : 0) + " S   " + juce::String::charToString (0x00B7) + "   OCTAVE-DOWN SHIMMER",
                             fnt::value (16.0f), { 476.0f, 412.0f, 358.0f, 30.0f }, juce::Justification::centred, col::emberHot, 0.3f);
            g.setColour (col::rim.withAlpha (0.7f));
            g.drawLine (380.0f, 398.0f, 820.0f, 398.0f, 0.8f);
            abyss::glowText (g, "TAILS RING OUT WHEN SWITCHED OFF", fnt::label (12.0f, 0.18f), { centrePanel.getX(), 600.0f, centrePanel.getWidth(), 16.0f },
                             juce::Justification::centred, col::ash, 0.0f);
        };
    }

    // ---- BAND: the legion (kick + bass that follow the riff) ---------------------------
    {
        auto& p = *makePanel (band, "THE LEGION");
        addToggle (p, "legionOn", "ENGAGE", { 372.0f, 236.0f, 104.0f, 30.0f }, "The Legion on / off: a kick and a bass that play along with your riff");
        const char* modes[] = { "CHUGS", "ALL NOTES" };
        const char* modeTips[] = { "The kick follows the low-string notes (the chugs) and ignores leads",
                                   "The kick follows every picked note" };
        for (int i = 0; i < 2; ++i)
        {
            auto b = std::make_unique<abyss::GlowButton> (modes[i], abyss::GlowButton::Style::tab);
            b->setBounds (juce::Rectangle<float> (i == 0 ? 674.0f : 752.0f, 236.0f, i == 0 ? 74.0f : 84.0f, 30.0f).toNearestInt());
            b->setTooltip (modeTips[i]);
            b->onClick = [this, i] { setParam ("kickMode", (float) i, "Kick follows"); };
            p.addAndMakeVisible (*b);
            kickModeButtons[(size_t) i] = b.get();
            owned.push_back (std::move (b));
        }
        auto strip = std::make_unique<abyss::HitStrip>();
        strip->setBounds (372, 280, 456, 62);
        p.addAndMakeVisible (*strip);
        hitStrip = strip.get();
        owned.push_back (std::move (strip));

        const float d = 50.0f, y = 446.0f;
        addKnob (p, "kickLevel", "KICK", { 412.0f, y }, d, "Kick level");
        addKnob (p, "kickFeel", "FEEL", { 508.0f, y }, d, "Feel: how light a pick attack still gets a kick");
        addKnob (p, "kickTone", "TONE", { 604.0f, y }, d, "Kick tone: deep and round to tight and clicky");
        addKnob (p, "bassLevel", "BASS", { 712.0f, y }, d, "Bass level: the riff doubled an octave under the guitar");
        addKnob (p, "bassGrit", "GRIT", { 808.0f, y }, d, "Bass grit: clean sub to driven mids");

        // the last riff, as files to drag into the DAW
        const struct { const char* name; const char* sub; Icon icon; int which; } tiles[] = {
            { "RIFF DI", "DRAG INTO YOUR DAW", Icon::load, 0 },
            { "BASS", "DRAG INTO YOUR DAW", Icon::echo, 1 },
            { "KICK MIDI", "ONTO YOUR DRUMS", Icon::kick, 2 } };
        for (int i = 0; i < 3; ++i)
        {
            auto tile = std::make_unique<DragTile> (tiles[i].name, tiles[i].sub, tiles[i].icon);
            tile->setBounds (juce::Rectangle<float> (372.0f + (float) i * 154.0f, 524.0f, 148.0f, 50.0f).toNearestInt());
            tile->setTooltip ("The last riff you played (up to 30 s, silence trimmed). Drag it into your DAW, or click to show the file.");
            const int which = tiles[i].which;
            auto* raw = tile.get();
            tile->makeFile = [this, which, raw]
            {
                const auto files = proc.exportRiff();
                if (! files.ok)
                {
                    raw->setStatus ("PLAY A RIFF FIRST");
                    return juce::File();
                }
                raw->setStatus (juce::String (files.seconds, 1) + " S" + (which == 2 ? "  " + juce::String::charToString (0x00B7) + "  "
                                                                                       + juce::String (files.numKicks) + " HITS" : juce::String()));
                raw->pulse();
                return which == 0 ? files.di : (which == 1 ? files.bass : files.kicks);
            };
            p.addAndMakeVisible (*tile);
            dragTiles[(size_t) i] = tile.get();
            owned.push_back (std::move (tile));
        }

        p.extra = [] (juce::Graphics& g)
        {
            abyss::glowText (g, "EVERY HIT YOU PLAY, IN TIME", fnt::label (11.0f, 0.2f), { 372.0f, 344.0f, 456.0f, 14.0f },
                             juce::Justification::centredLeft, col::ash, 0.0f);
            for (auto [x0, x1, name] : { std::tuple<float, float, const char*> { 372.0f, 644.0f, "KICK" }, { 672.0f, 846.0f, "BASS" } })
            {
                const float yy = 500.0f;
                g.setColour (col::rim);
                g.drawLine (x0, yy, x1, yy, 0.8f);
                abyss::glowText (g, name, fnt::serif (11.0f, 0.4f), { x0, yy - 16.0f, x1 - x0, 12.0f }, juce::Justification::centred, col::ash, 0.0f);
            }
            abyss::glowText (g, "PLAY A RIFF. THE LEGION FOLLOWS.", fnt::serifLight (12.0f, 0.42f), { centrePanel.getX(), 600.0f, centrePanel.getWidth(), 18.0f },
                             juce::Justification::centred, col::ember, 0.3f);
        };
    }

    // ---- CAB (the chamber is always on the right; its centre view lists the IRs) ------
    {
        auto& p = *makePanel (cab, "THE CHAMBER");
        const char* names[] = { "CINDER 4x12", "IRON 4x12", "OBSIDIAN 4x12", "USER IR" };
        for (int i = 0; i < 4; ++i)
        {
            auto b = std::make_unique<abyss::GlowButton> (Icon::cab, names[i], abyss::GlowButton::Style::tab);
            b->setBounds (juce::Rectangle<float> (440.0f, 256.0f + (float) i * 52.0f, 320.0f, 42.0f).toNearestInt());
            b->onClick = [this, i]
            {
                if (i == 3 && ! proc.hasUserIr()) { loadIr(); return; }
                setParam ("ir", (float) i, "Cabinet");
            };
            p.addAndMakeVisible (*b);
            owned.push_back (std::move (b));
        }
        auto load = std::make_unique<abyss::GlowButton> (Icon::load, "LOAD IR", abyss::GlowButton::Style::tab);
        load->setBounds (520, 476, 160, 36);
        load->onClick = [this] { loadIr(); };
        p.addAndMakeVisible (*load);
        owned.push_back (std::move (load));
        p.extra = [] (juce::Graphics& g)
        {
            abyss::glowText (g, "4x12 IMPULSE RESPONSES  " + juce::String::charToString (0x00B7) + "  OR YOUR OWN", fnt::label (12.0f, 0.16f),
                             { centrePanel.getX(), 600.0f, centrePanel.getWidth(), 16.0f }, juce::Justification::centred, col::ash, 0.0f);
        };
    }
}

//==============================================================================
void ThallbyssalEditor::buildCabChamber()
{
    auto& host = *backdrop;

    auto pt = std::make_unique<abyss::Portal> (abyss::Portal::Style::chamber);
    pt->setBounds (square (portalCentre, 176.0f).toNearestInt());
    host.addAndMakeVisible (*pt);
    portal = pt.get();
    owned.push_back (std::move (pt));

    auto prev = std::make_unique<abyss::GlowButton> (Icon::previous, juce::String(), abyss::GlowButton::Style::text);
    auto next = std::make_unique<abyss::GlowButton> (Icon::next, juce::String(), abyss::GlowButton::Style::text);
    auto load = std::make_unique<abyss::GlowButton> (Icon::load, juce::String(), abyss::GlowButton::Style::icon);
    prev->setBounds (886, 480, 26, 32);
    next->setBounds (1102, 480, 26, 32);
    load->setBounds (1130, 480, 32, 32);
    prev->setTooltip ("Previous impulse response");
    next->setTooltip ("Next impulse response");
    load->setTooltip ("Load your own impulse response (.wav / .aiff)");
    prev->onClick = [this] { stepParam ("ir", -1, 0, proc.hasUserIr() ? 3 : 2); };
    next->onClick = [this] { stepParam ("ir", 1, 0, proc.hasUserIr() ? 3 : 2); };
    load->onClick = [this] { loadIr(); };
    for (auto* b : { prev.get(), next.get(), load.get() })
        host.addAndMakeVisible (b);

    auto name = std::make_unique<TextField> (false, Icon::cab, false);
    name->setBounds (914, 480, 186, 32);
    name->onClick = [this] { selectModule (cab); };
    name->setTooltip ("Impulse response (click to see them all)");
    host.addAndMakeVisible (*name);
    irName = name.get();
    owned.push_back (std::move (prev));
    owned.push_back (std::move (next));
    owned.push_back (std::move (load));
    owned.push_back (std::move (name));

    auto values = std::make_unique<StatusCell>();
    values->setBounds (892, 522, 262, 22);
    host.addAndMakeVisible (*values);
    cabValues = values.get();
    owned.push_back (std::move (values));

    auto mix = std::make_unique<abyss::LinearSlider>();
    mix->setBounds (884, 544, 278, 26);
    mix->setPopupParent (&stage);
    mix->setTooltip ("Cab mix: dry amp to full cabinet");
    host.addAndMakeVisible (*mix);
    sliderAttachments.push_back (abyss::attach (*mix, *apvts.getParameter ("cabMix"), &proc.undoManager));
    cabMix = mix.get();
    owned.push_back (std::move (mix));

    addKnob (host, "lowCut", {}, { 946.0f, 608.0f }, 40.0f, "Low cut after the cab");
    addKnob (host, "fizz", {}, { 1022.0f, 608.0f }, 40.0f, "Fizz tamer: pulls back whistling resonances in the treble only while they ring, leaves the attack and brightness alone");
    addKnob (host, "highCut", {}, { 1098.0f, 608.0f }, 40.0f, "High cut after the cab (off at the top)");
    for (auto [x, slot] : { std::pair<float, StatusCell**> { 946.0f, &lowCutCaption }, { 1022.0f, &fizzCaption }, { 1098.0f, &highCutCaption } })
    {
        auto c = std::make_unique<StatusCell> (true);
        c->setBounds (juce::Rectangle<float> (x - 38.0f, 634.0f, 76.0f, 30.0f).toNearestInt());
        host.addAndMakeVisible (*c);
        *slot = c.get();
        owned.push_back (std::move (c));
    }
}

//==============================================================================
void ThallbyssalEditor::buildChain()
{
    auto& host = *backdrop;
    const struct { Module m; const char* name; Icon icon; } order[] = {
        { drop, "DROP", Icon::drop }, { gate, "GATE", Icon::gate }, { boost, "BOOST", Icon::boost }, { amp, "AMP", Icon::amp },
        { shape, "SHAPE", Icon::shape }, { cab, "CAB", Icon::cab }, { fx, "FX", Icon::fx }, { band, "BAND", Icon::kick } };
    constexpr int count = (int) std::size (order);

    const float x0 = 112.0f, x1 = 1088.0f, w = 98.0f, y = 686.0f, h = 78.0f;
    const float gap = (x1 - x0 - (float) count * w) / (float) (count + 1);

    auto in = std::make_unique<abyss::Jack> ("INPUT");
    in->setBounds (40, 684, 70, 82);
    in->setClickingTogglesState (false);
    in->setTooltip ("Input");
    host.addAndMakeVisible (*in);
    inputJack = in.get();
    owned.push_back (std::move (in));

    auto out = std::make_unique<abyss::Jack> ("OUTPUT");
    out->setBounds (1090, 684, 70, 82);
    out->setClickingTogglesState (true);
    out->setTooltip ("Power: click to bypass the whole rig");
    host.addAndMakeVisible (*out);
    outputJack = out.get();
    out->onClick = [this]
    {
        // the output ring is the power switch (bypass inverted)
        setParam ("bypass", outputJack->getToggleState() ? 0.0f : 1.0f, "Power");
    };
    owned.push_back (std::move (out));

    for (int i = 0; i <= count; ++i)
    {
        auto link = std::make_unique<abyss::ChainLink>();
        const float lx = x0 + (float) i * (w + gap);
        link->setBounds (juce::Rectangle<float> (lx - 4.0f, y + 24.0f, gap + 8.0f, 30.0f).toNearestInt());
        link->setInterceptsMouseClicks (false, false);
        host.addAndMakeVisible (*link);
        links[(size_t) i] = link.get();
        owned.push_back (std::move (link));
    }
    for (int i = 0; i < count; ++i)
    {
        auto block = std::make_unique<abyss::ChainBlock> (order[i].name, order[i].icon);
        block->setBounds (juce::Rectangle<float> (x0 + gap + (float) i * (w + gap), y, w, h).toNearestInt());
        const int m = order[i].m;
        block->onSelect = [this, m] { selectModule (m); };
        block->onToggle = [this, m] { toggleModule (m); };
        block->setTooltip ("Click to edit, click the bar to switch on / off");
        host.addAndMakeVisible (*block);
        blocks[(size_t) m] = block.get();
        owned.push_back (std::move (block));
    }
}

void ThallbyssalEditor::buildStatus()
{
    auto& host = *backdrop;
    auto add = [&] (auto component, juce::Rectangle<int> r)
    {
        auto* raw = component.get();
        raw->setBounds (r);
        host.addAndMakeVisible (*raw);
        owned.push_back (std::move (component));
        return raw;
    };

    pressureGauge = add (std::make_unique<abyss::Gauge>(), { 44, 798, 58, 58 });
    pressureCell  = add (std::make_unique<StatusCell>(), { 112, 794, 152, 64 });
    pressureBar   = add (std::make_unique<abyss::HBar>(), { 112, 822, 152, 8 });

    signalCell = add (std::make_unique<StatusCell>(), { 290, 794, 238, 64 });
    scope      = add (std::make_unique<abyss::Scope>(), { 290, 814, 238, 26 });

    ring = add (std::make_unique<abyss::Portal> (abyss::Portal::Style::ring), { 556, 788, 88, 88 });

    depthCell = add (std::make_unique<StatusCell>(), { 672, 794, 186, 64 });
    depthBar  = add (std::make_unique<abyss::HBar>(), { 672, 822, 186, 8 });

    outputCell  = add (std::make_unique<StatusCell>(), { 884, 794, 112, 64 });
    outputGauge = add (std::make_unique<abyss::Gauge>(), { 998, 800, 48, 48 });

    hotCell = add (std::make_unique<StatusCell>(), { 1062, 794, 100, 64 });
    pressureCell->setTooltip ("How hard the amp is hit (input after trim and gain)");
}

//==============================================================================
void ThallbyssalEditor::selectModule (int m)
{
    m = juce::jlimit (0, numModules - 1, m);
    selected = m;
    apvts.state.setProperty ("abyssModule", m, nullptr);
    for (int i = 0; i < numModules; ++i)
    {
        if (panels[(size_t) i] != nullptr)
            panels[(size_t) i]->setVisible (i == m);
        if (blocks[(size_t) i] != nullptr)
            blocks[(size_t) i]->setSelected (i == m);
    }
}

void ThallbyssalEditor::toggleModule (int m)
{
    auto flip = [this] (const char* id, const juce::String& name) { setParam (id, param (id) > 0.5f ? 0.0f : 1.0f, name); };
    switch (m)
    {
        case drop:  flip ("dropOn", "Drop"); break;
        case gate:  flip ("gateOn", "Gate"); break;
        case boost: flip ("boostOn", "Boost"); break;
        case shape: flip ("shapeOn", "Shape"); break;
        case amp:   break;   // the amp is the rig: power lives on the output ring
        case band:  flip ("legionOn", "Legion"); break;
        case cab:
        {
            const float mix = param ("cabMix");
            if (mix > 0.5f) { lastCabMix = mix; setParam ("cabMix", 0.0f, "Cab"); }
            else setParam ("cabMix", lastCabMix > 0.5f ? lastCabMix : 100.0f, "Cab");
            break;
        }
        case fx:
        {
            const bool echo = param ("delayOn") > 0.5f, verb = param ("reverbOn") > 0.5f;
            proc.undoManager.beginNewTransaction ("FX");
            if (echo || verb)
            {
                fxRememberEcho = echo;
                fxRememberAbyss = verb;
                setParam ("delayOn", 0.0f);
                setParam ("reverbOn", 0.0f);
            }
            else
            {
                setParam ("delayOn", fxRememberEcho ? 1.0f : 0.0f);
                setParam ("reverbOn", fxRememberAbyss || ! fxRememberEcho ? 1.0f : 0.0f);
            }
            break;
        }
        default: break;
    }
}

void ThallbyssalEditor::selectRig (int position)
{
    if (position == 4 && ! proc.hasUserRig())
    {
        loadRig();
        return;
    }
    proc.undoManager.beginNewTransaction ("Rig");
    if (position == 3)
        setParam ("rigMode", 1.0f);
    else
    {
        setParam ("rigMode", 0.0f);
        setParam ("rig", position == 4 ? 3.0f : (float) position);
    }
}

void ThallbyssalEditor::bindEchoTime()
{
    if (echoTimeKnob == nullptr)
        return;
    const bool synced = param ("delaySync") > 0.5f;
    echoTimeAttachment.reset();
    echoTimeAttachment = abyss::attach (*echoTimeKnob, *apvts.getParameter (synced ? "delayDiv" : "delayTime"), &proc.undoManager);
    echoTimeKnob->setTooltip (synced ? "Echo time as a note value of the host tempo" : "Echo time in milliseconds");
    echoTimeKnob->repaint();
}

void ThallbyssalEditor::tapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! taps.empty() && now - taps.back() > 2000.0)
        taps.clear();
    taps.push_back (now);
    if (taps.size() > 5)
        taps.erase (taps.begin());
    if (taps.size() < 2)
        return;
    const double interval = (taps.back() - taps.front()) / (double) (taps.size() - 1);
    proc.undoManager.beginNewTransaction ("Tap tempo");
    setParam ("delaySync", 0.0f);
    setParam ("delayTime", (float) juce::jlimit (20.0, 2000.0, interval));
}

void ThallbyssalEditor::showTargetMenu()
{
    juce::PopupMenu menu;
    const int current = juce::roundToInt (param ("inputTarget"));
    menu.addItem (1, "OPEN  " + juce::String::charToString (0x00B7) + "  peaks at " + minus ("-12") + " dBFS", true, current == 0);
    menu.addItem (2, "MODERN  " + juce::String::charToString (0x00B7) + "  peaks at " + minus ("-9") + " dBFS", true, current == 1);
    menu.addItem (3, "HOT  " + juce::String::charToString (0x00B7) + "  peaks at " + minus ("-6") + " dBFS", true, current == 2);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (targetZone), [this] (int result)
    {
        if (result > 0)
            setParam ("inputTarget", (float) (result - 1), "Target zone");
    });
}

void ThallbyssalEditor::loadRig()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a NAM capture", juce::File(), "*.nam");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile() && proc.loadUserRig (file))
                                  selectRig (4);
                          });
}

void ThallbyssalEditor::loadIr()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a cabinet impulse response", juce::File(), "*.wav;*.aiff;*.aif;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile() && proc.loadUserIr (file))
                                  setParam ("ir", 3.0f, "Load IR");
                          });
}

void ThallbyssalEditor::addSettingsItems (juce::PopupMenu& menu)
{
    menu.addSeparator();
    menu.addItem (100, "Load NAM rig" + juce::String::charToString (0x2026));
    menu.addItem (101, "Load cabinet IR" + juce::String::charToString (0x2026));
    menu.addSeparator();
    const float trim = param ("inputTrim");
    menu.addItem (102, "Calibrate DI (Auto Input)");
    menu.addItem (103, "Reset input trim (" + minus (juce::String (trim, 1)) + " dB)", std::abs (trim) > 0.05f);
    menu.addSeparator();
    menu.addItem (104, (proc.licensing->getStatus (proc.licenceProduct).licensed ? "Licence" : "Unlock Thallbyssal")
                           + juce::String::charToString (0x2026));
}

void ThallbyssalEditor::handleSettingsItem (int id)
{
    if (id == 100) loadRig();
    if (id == 101) loadIr();
    if (id == 102) proc.startAutoInput();
    if (id == 103) setParam ("inputTrim", 0.0f, "Reset input trim");
    if (id == 104) showUnlock();
}

void ThallbyssalEditor::showUnlock()
{
    if (unlock == nullptr)
    {
        unlock = std::make_unique<apex::ui::UnlockOverlay> (*proc.licensing, proc.licenceProduct, "Thallbyssal", APEX_STORE_URL);
        unlock->onClose = [this]
        {
            // the close button belongs to the overlay: delete it after its click has returned
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ThallbyssalEditor> (this)]
                                             {
                                                 if (safe != nullptr)
                                                     safe->unlock.reset();
                                             });
        };
        unlock->setBounds (0, 0, designWidth, designHeight);
        backdrop->addAndMakeVisible (*unlock);
    }
    unlock->toFront (true);
}

void ThallbyssalEditor::updateTrialBadge()
{
    const auto s = proc.licensing->getStatus (proc.licenceProduct);
    const auto dot = juce::String::charToString (0x00B7);
    const juce::String text = s.licensed ? juce::String()
                              : s.expired ? "TRIAL ENDED  " + dot + "  UNLOCK"
                                          : "TRIAL  " + dot + "  " + juce::String (s.daysLeft) + (s.daysLeft == 1 ? " DAY LEFT" : " DAYS LEFT");
    if (text != trialText)
    {
        trialText = text;
        trialBadge->setButtonText (text);
        trialBadge->setVisible (text.isNotEmpty());
        trialBadge->setPulsing (s.expired);
        trialBadge->setTooltip (s.expired ? "The trial has ended: your DI passes through untouched. Click to enter a licence key."
                                          : "Everything works during the trial. Click to enter a licence key.");
    }
    if (s.expired)
        trialBadge->setPulse (0.55f + 0.45f * std::sin ((float) frameCounter * 0.12f));
}

float ThallbyssalEditor::getInputPeak()  { return proc.inputMagnitude.load(); }
float ThallbyssalEditor::getOutputPeak() { return proc.outputMagnitude.load(); }

//==============================================================================
void ThallbyssalEditor::tick()
{
    ++frameCounter;
    const float inLin = proc.inputMagnitude.load(), outLin = proc.outputMagnitude.load();
    inDb = toDb (inLin);
    outDb = toDb (outLin);
    const bool power = param ("bypass") < 0.5f;
    const bool playing = inDb > -50.0f;

    // ---- header ----------------------------------------------------------------
    updateTrialBadge();
    presetName->setText (presets.getCurrentName().toUpperCase(), presets.isModified());
    aButton->setToggleState (presets.getSlot() == 0, juce::dontSendNotification);
    bButton->setToggleState (presets.getSlot() == 1, juce::dontSendNotification);
    undoButton->setEnabled (proc.undoManager.canUndo());
    redoButton->setEnabled (proc.undoManager.canRedo());
    tunerButton->setToggleState (isTunerOpen(), juce::dontSendNotification);
    inMeter->pushLevel (inDb);
    outMeter->pushLevel (outDb);

    // ---- input match -----------------------------------------------------------------
    const float trim = param ("inputTrim"), target = proc.getInputTargetDb();
    const float relative = inDb + trim - target;
    diMeter->pushLevel (playing ? relative : -100.0f);
    radar->push (inDb + trim, target, playing);
    silentFrames = playing ? 0 : silentFrames + 1;
    matchPeak = playing ? juce::jmax (relative, matchPeak - 0.15f) : matchPeak;

    const bool listening = proc.isAutoInputListening();
    const float pulse = 0.5f + 0.5f * std::sin ((float) frameCounter * 0.25f);
    calibrate->setPulse (listening ? pulse : 0.0f);
    calibrate->setButtonText (listening ? "LISTENING" + juce::String::charToString (0x2026) : "CALIBRATE DI");
    const bool calibrated = (bool) apvts.state.getProperty ("calibrated", false) || std::abs (trim) > 0.05f;
    if (proc.getAutoInputOutcome() == 1 && ! calibrated)
        apvts.state.setProperty ("calibrated", true, nullptr);
    if (! calibrated && ! listening)
        calibrate->setPulse (0.35f + 0.35f * pulse);   // first run: start here
    if (listening)
        matchStatus->set ("PLAY YOUR HEAVIEST RIFF", false, pulse);
    else if (! calibrated)
        matchStatus->set ("STEP 1: CALIBRATE YOUR DI", false, 0.5f + 0.5f * pulse);
    else if (silentFrames > 60)
        matchStatus->set (proc.getAutoInputOutcome() == 2 ? "NO GUITAR HEARD" : "WAITING FOR SIGNAL", false, 0.0f);
    else if (std::abs (matchPeak) <= 3.0f)
        matchStatus->set ("OPTIMAL", true, 0.3f);
    else
        matchStatus->set (matchPeak > 0.0f ? "TOO HOT  " + juce::String::charToString (0x00B7) + "  CALIBRATE"
                                           : "TOO LOW  " + juce::String::charToString (0x00B7) + "  CALIBRATE", false, 0.2f);

    const char* zones[] = { "OPEN", "MODERN", "HOT" };
    targetZone->setText (juce::String (zones[juce::jlimit (0, 2, juce::roundToInt (param ("inputTarget")))]) + " ZONE  "
                         + juce::String::charToString (0x00B7) + "  " + minus (juce::String ((int) target)) + " dBFS");

    // ---- modules ------------------------------------------------------------------
    const bool blend = param ("rigMode") > 0.5f;
    const int rig = juce::roundToInt (param ("rig"));
    const int position = blend ? 3 : (rig == 3 ? 4 : juce::jlimit (0, 2, rig));
    for (int i = 0; i < 5; ++i)
        rigButtons[(size_t) i]->setToggleState (i == position, juce::dontSendNotification);
    for (auto* k : blendTrims)
        k->setVisible (blend);
    if (punchBar != nullptr)
        punchBar->setValue (param ("shapeOn") > 0.5f ? proc.chugPunch.load() : 0.0f);
    {
        const std::uint32_t total = proc.getLegionHits();
        if (total != seenHits)
        {
            hitStrip->addHit (proc.getLegionLastVelocity());
            seenHits = total;
        }
        if (selected == band)
            hitStrip->advance (1.0f / (4.0f * 30.0f));   // four seconds across
        for (auto* t : dragTiles)
            t->tick();
        const int kickMode = juce::roundToInt (param ("kickMode"));
        for (int i = 0; i < 2; ++i)
            kickModeButtons[(size_t) i]->setToggleState (i == kickMode, juce::dontSendNotification);
    }
    // live readouts in the panel (drop display, gate state, echo time, blend trims)
    {
        juce::String sig;
        sig << selected << param ("dropShift") << param ("dropOn") << proc.getLatencyMs() << (inDb > param ("gate") ? 1 : 0)
            << param ("gateOn") << proc.getEchoMs() << param ("delaySync") << param ("delayDiv") << param ("reverbDecay")
            << proc.getHostBpm() << param ("rigMode");
        if (sig != panelSignature)
        {
            panelSignature = sig;
            if (auto* panel = panels[(size_t) selected])
                panel->repaint (centrePanel.toNearestInt());
        }
    }

    // ---- cab chamber ----------------------------------------------------------------------
    {
        auto* ir = apvts.getParameter ("ir");
        const int index = juce::roundToInt (param ("ir"));
        const auto name = ir->getText (ir->convertTo0to1 ((float) index), 32).toUpperCase();
        irName->setText (juce::String (index + 1).paddedLeft ('0', 2) + "  " + juce::String::charToString (0x00B7) + "  "
                         + (index == 3 && ! proc.hasUserIr() ? juce::String ("NO FILE") : name));
        cabValues->set ("CAB MIX", juce::String (juce::roundToInt (param ("cabMix"))) + "%", {});
        lowCutCaption->set ("LOW CUT", juce::String (juce::roundToInt (param ("lowCut"))) + " Hz", {});
        fizzCaption->set ("FIZZ", param ("fizz") < 0.5f ? juce::String ("OFF") : juce::String (juce::roundToInt (param ("fizz"))) + "%", {});
        const float hc = param ("highCut");
        highCutCaption->set ("HIGH CUT", hc >= 19500.0f ? juce::String ("OFF") : juce::String (hc / 1000.0f, 1) + " kHz", {});
        portal->setIntensity (power ? juce::jlimit (0.0f, 1.0f, (outDb + 48.0f) / 42.0f) * (param ("cabMix") / 100.0f) : 0.0f);
    }

    // ---- chain ---------------------------------------------------------------------
    const float inAct = power ? juce::jlimit (0.0f, 1.0f, (inDb + 48.0f) / 42.0f) : 0.0f;
    const float outAct = power ? juce::jlimit (0.0f, 1.0f, (outDb + 48.0f) / 42.0f) : 0.0f;
    const bool moduleOn[numModules] = { param ("dropOn") > 0.5f, param ("gateOn") > 0.5f, param ("boostOn") > 0.5f, power,
                                        param ("shapeOn") > 0.5f, param ("cabMix") > 0.5f,
                                        param ("delayOn") > 0.5f || param ("reverbOn") > 0.5f, param ("legionOn") > 0.5f };
    {
        const int st = juce::roundToInt (param ("dropShift"));
        const char* rigs[] = { "BITE", "BODY", "EDGE", "USER" };
        const bool echoOn = param ("delayOn") > 0.5f, abyssOn = param ("reverbOn") > 0.5f;
        auto* ir = apvts.getParameter ("ir");
        blocks[drop]->setValueText (noteName (4 + st) + (st <= -12 ? minus (" -1 OCT") : juce::String (" STD")));
        blocks[gate]->setValueText (minus (juce::String (juce::roundToInt (param ("gate")))) + " dB");
        blocks[boost]->setValueText ("DRIVE " + juce::String (juce::roundToInt (param ("boostDrive"))));
        blocks[amp]->setValueText (param ("rigMode") > 0.5f ? juce::String ("BLEND") : juce::String (rigs[juce::jlimit (0, 3, juce::roundToInt (param ("rig")))]));
        blocks[shape]->setValueText ("CHUG " + juce::String (juce::roundToInt (param ("chug"))));
        blocks[cab]->setValueText (ir->getCurrentValueAsText().toUpperCase());
        blocks[band]->setValueText (param ("legionOn") > 0.5f ? (param ("kickMode") > 0.5f ? juce::String ("ALL NOTES") : juce::String ("CHUGS"))
                                                              : juce::String ("OFF"));
        blocks[fx]->setValueText (echoOn && abyssOn ? juce::String ("ECHO + ABYSS") : (echoOn ? juce::String ("ECHO") : (abyssOn ? juce::String ("ABYSS") : juce::String ("OFF"))));
    }
    for (int m = 0; m < numModules; ++m)
    {
        blocks[(size_t) m]->setEnabledState (moduleOn[m] && power);
        blocks[(size_t) m]->setActivity (m <= amp ? inAct : outAct);
    }
    for (int i = 0; i <= numModules; ++i)
        links[(size_t) i]->setLevel (i <= 3 ? inAct : outAct);
    inputJack->setLevel (inAct);
    outputJack->setToggleState (power, juce::dontSendNotification);
    outputJack->setLevel (outAct);

    // ---- status bar ----------------------------------------------------------------------
    const float driveDb = toDb (proc.ampDrive.load());
    const float pressure = juce::jlimit (0.0f, 1.0f, (driveDb + 36.0f) / 42.0f);
    pressureGauge->setValue (pressure);
    pressureBar->setValue (pressure);
    pressureCell->set ("AMP DRIVE", juce::String (juce::roundToInt (pressure * 100.0f)) + "%", "HOW HARD THE AMP IS HIT");

    if (frameCounter % 2 == 0)
    {
        std::vector<float> samples (240);
        const int w = proc.scopeWrite.load (std::memory_order_acquire);
        for (int i = 0; i < 240; ++i)
            samples[(size_t) i] = proc.scope[(size_t) ((w - 240 + i + ApexAmpProcessor::scopeSize) % ApexAmpProcessor::scopeSize)].load (std::memory_order_relaxed);
        scope->setSamples (samples);
    }
    signalCell->set ("SIGNAL", "DSP " + juce::String (proc.dspLoad.load() * 100.0f, 1) + "%",
                     "LATENCY " + juce::String (proc.getLatencyMs(), 1) + " MS");
    ring->setIntensity (outAct);

    const int st = juce::roundToInt (param ("dropShift"));
    const bool dropOn = param ("dropOn") > 0.5f;
    depthBar->setValue (dropOn ? (float) std::abs (st) / 12.0f : 0.0f);
    depthCell->set ("TUNING", dropOn ? noteName (4 + st) + (st <= -12 ? minus (" -1 OCT") : juce::String (" STD")) : juce::String ("E STD"),
                    dropOn ? "DROP " + minus ((st > 0 ? "+" : "") + juce::String (st)) + " ST  " + juce::String::charToString (0x00B7) + "  "
                                 + juce::String (proc.getLatencyMs(), 1) + " MS"
                           : juce::String ("DROP OFF  ") + juce::String::charToString (0x00B7) + "  CLICK DROP");

    outputGauge->setValue (juce::jlimit (0.0f, 1.0f, (outDb + 48.0f) / 48.0f));
    outputCell->set ("OUTPUT", outDb > -99.0f ? minus (juce::String (outDb, 1)) + " dB" : juce::String::charToString (0x2212) + juce::String::charToString (0x221e),
                     "PEAK LEVEL");

    if (outDb >= hotPeak) { hotPeak = outDb; hotFrames = 45; }
    else if (--hotFrames < 0) hotPeak = juce::jmax (outDb, hotPeak - 0.5f);
    const bool hot = hotPeak > -1.0f;
    hotCell->set (hot ? "HOT" : "CLEAN", hotPeak > -99.0f ? minus (juce::String (hotPeak, 1)) + " dB" : juce::String(), hot ? "LOWER THE MASTER" : "HEADROOM OK", hot);
}
