#include "AmpEditor.h"
#include "apex/ui/Objects.h"
#include <array>
#include "apex/ui/Theme.h"

using namespace apex::ui;

namespace
{
    constexpr int designWidth = 1200, designHeight = 760;

    const juce::Colour dropPaint  { 0xffd2611f };
    const juce::Colour gatePaint  { 0xff353b44 };
    const juce::Colour boostPaint { 0xff1f6040 };
    const juce::Colour shapePaint { 0xff7a1f25 };
    const juce::Colour voidPaint  { 0xff2c2545 };
    const juce::Colour dropInk    { 0xff1c0e06 };

    juce::Rectangle<float> square (juce::Point<float> c, float d) { return juce::Rectangle<float> (d, d).withCentre (c); }

    juce::String noteName (int semitone)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[((semitone % 12) + 12) % 12];
    }

    juce::String dot() { return juce::String::charToString (0x00B7); }
}

//==============================================================================
AmpEditor::Layout::Layout()
{
    head     = { 40.0f, 8.0f, 1120.0f, 292.0f };
    plate    = draw::ampHeadFaceplate (head);
    cab      = { 22.0f, 280.0f, 1156.0f, 520.0f };
    cabPlate = { 290.0f, 316.0f, 620.0f, 98.0f };
    board    = { 40.0f, 432.0f, 1120.0f, 270.0f };

    // pedals in signal order, centred on the board
    const float y = 448.0f, h = 236.0f, gap = 16.0f;
    float x = 70.0f;
    drop     = { x, y, 226.0f, h }; x += 226.0f + gap;
    gate     = { x, y, 118.0f, h }; x += 118.0f + gap;
    boost    = { x, y, 150.0f, h }; x += 150.0f + gap;
    shape    = { x, y, 150.0f, h }; x += 150.0f + gap;
    voidUnit = { x, y, 352.0f, h };
    dropTop  = draw::pedalTopFace (drop);
    gateTop  = draw::pedalTopFace (gate);
    boostTop = draw::pedalTopFace (boost);
    shapeTop = draw::pedalTopFace (shape);
    voidTop  = draw::pedalTopFace (voidUnit);

    // ---- faceplate --------------------------------------------------------------
    const float ky = plate.getCentreY() - 8.0f;
    float kx = plate.getX() + 410.0f;
    gain     = { kx, ky }; kx += 106.0f;
    tight    = { kx, ky }; kx += 86.0f;
    bass     = { kx, ky }; kx += 86.0f;
    mid      = { kx, ky }; kx += 86.0f;
    treble   = { kx, ky }; kx += 86.0f;
    presence = { kx, ky }; kx += 106.0f;
    master   = { kx, ky };

    const juce::Point<float> selCentre (plate.getX() + 262.0f, plate.getCentreY() + 4.0f);
    selector = { selCentre.x - 86.0f, selCentre.y - 93.0f, 172.0f, 150.0f };
    trims    = { selCentre.x - 80.0f, plate.getBottom() - 56.0f, 160.0f, 50.0f };
    power    = { plate.getX() + 96.0f, plate.getBottom() - 86.0f, 30.0f, 70.0f };
    jewel    = square ({ plate.getX() + 45.0f, plate.getBottom() - 51.0f }, 80.0f);

    // ---- cabinet plate ---------------------------------------------------------------
    const auto& cp = cabPlate;
    cabGlass  = { cp.getX() + 22.0f, cp.getY() + 14.0f, 240.0f, 44.0f };
    cabLcd    = cabGlass.reduced (12.0f, 3.0f);
    cabPrev   = { cp.getX() + 22.0f, cp.getY() + 64.0f, 34.0f, 24.0f };
    cabNext   = { cp.getX() + 60.0f, cp.getY() + 64.0f, 34.0f, 24.0f };
    cabLoad   = { cabGlass.getRight() - 34.0f, cp.getY() + 64.0f, 34.0f, 24.0f };
    cabMix    = { cp.getX() + 330.0f, cp.getY() + 40.0f };
    cabLowCut = { cp.getX() + 430.0f, cp.getY() + 40.0f };

    // ---- drop ---------------------------------------------------------------------
    const auto& d = dropTop;
    dropGlass   = { d.getX() + 10.0f, d.getY() + 10.0f, 148.0f, 70.0f };
    dropDigits  = dropGlass.reduced (14.0f, 8.0f).withTrimmedBottom (18.0f);
    dropCaption = { dropGlass.getX() + 4.0f, dropGlass.getBottom() - 22.0f, dropGlass.getWidth() - 8.0f, 18.0f };
    dropUp      = { dropGlass.getRight() + 8.0f, d.getY() + 10.0f, 30.0f, 33.0f };
    dropDown    = { dropGlass.getRight() + 8.0f, d.getY() + 47.0f, 30.0f, 33.0f };
    dropBody    = { d.getX() + 38.0f, d.getY() + 124.0f };
    dropSub     = { d.getRight() - 38.0f, d.getY() + 124.0f };

    // ---- gate / boost / shape: one row of knobs -------------------------------------
    gateThresh = { gateTop.getX() + 28.0f, gateTop.getY() + 48.0f };
    gateHold   = { gateTop.getRight() - 28.0f, gateTop.getY() + 48.0f };
    auto row3 = [] (const juce::Rectangle<float>& t, juce::Point<float>& a, juce::Point<float>& b, juce::Point<float>& c)
    {
        a = { t.getX() + 25.0f, t.getY() + 48.0f };
        b = { t.getCentreX(), t.getY() + 48.0f };
        c = { t.getRight() - 25.0f, t.getY() + 48.0f };
    };
    row3 (boostTop, boostDrive, boostTone, boostLevel);
    row3 (shapeTop, chug, chugFreq, dirt);

    // ---- void ---------------------------------------------------------------------
    const auto& v = voidTop;
    voidGlass = { v.getX() + 10.0f, v.getY() + 10.0f, 210.0f, 46.0f };
    voidLcd   = voidGlass.reduced (10.0f, 4.0f);
    sync      = { voidGlass.getRight() + 10.0f, v.getY() + 18.0f, 44.0f, 30.0f };
    tap       = { sync.getRight() + 4.0f, v.getY() + 18.0f, 42.0f, 30.0f };
    const float ry = v.getY() + 100.0f;
    echoTime     = { v.getX() + 26.0f,  ry };
    echoFeedback = { v.getX() + 66.0f,  ry };
    echoDuck     = { v.getX() + 106.0f, ry };
    echoMix      = { v.getX() + 146.0f, ry };
    abyssDecay   = { v.getX() + 185.0f, ry };
    abyssDepth   = { v.getX() + 225.0f, ry };
    abyssTone    = { v.getX() + 265.0f, ry };
    abyssMix     = { v.getX() + 305.0f, ry };
    echoSwitch   = { v.getX() + 86.0f,  v.getBottom() - 32.0f };
    abyssSwitch  = { v.getX() + 245.0f, v.getBottom() - 32.0f };
}

//==============================================================================
struct AmpEditor::AmpStage : public juce::Component
{
    explicit AmpStage (const Layout& l) : layout (l)
    {
        setOpaque (true);
        setBufferedToImage (true);
        setInterceptsMouseClicks (false, true);
    }

    void faceplateKnob (juce::Graphics& g, juce::Point<float> c, float d, const juce::String& label, bool numbers)
    {
        draw::knobScale (g, c, d * 0.5f + 5.0f, colours::bone.withAlpha (0.85f), numbers);
        draw::silkscreen (g, label, Fonts::label (numbers ? 15.0f : 13.0f, 0.22f),
                          { c.x - 60.0f, c.y + d * 0.5f + (numbers ? 26.0f : 16.0f), 120.0f, 16.0f },
                          juce::Justification::centred, colours::bone);
    }

    void pedalLabel (juce::Graphics& g, juce::Point<float> knobCentre, float d, const juce::String& text, juce::Colour ink,
                     float size = 11.0f)
    {
        draw::silkscreen (g, text, Fonts::labelBold (size, 0.2f), { knobCentre.x - 40.0f, knobCentre.y - d * 0.5f - 18.0f, 80.0f, 12.0f },
                          juce::Justification::centred, ink);
    }

    void pedalName (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, juce::Colour ink, float size = 26.0f)
    {
        draw::silkscreen (g, name, Fonts::display (size, 0.2f), area, juce::Justification::centred, ink);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        draw::stage (g, b, { b.getCentreX(), 60.0f });

        // ---- 4x12 behind, head on top --------------------------------------------
        {
            const juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (getLocalBounds());
            draw::speakerCab (g, layout.cab);
        }
        paintCabPlate (g);

        const auto plate = draw::ampHead (g, layout.head);
        draw::faceplate (g, plate);
        draw::chromeText (g, "APEX", Fonts::display (56.0f, 0.16f), { plate.getX() + 26.0f, plate.getY() + 26.0f, 190.0f, 66.0f },
                          juce::Justification::centredLeft);
        draw::silkscreen (g, "NEURAL HIGH-GAIN", Fonts::label (11.0f, 0.34f), { plate.getX() + 30.0f, plate.getY() + 94.0f, 190.0f, 14.0f },
                          juce::Justification::centredLeft, colours::dim);
        draw::silkscreen (g, "POWER", Fonts::label (11.0f, 0.3f), { layout.power.getCentreX() - 40.0f, plate.getBottom() - 22.0f, 80.0f, 14.0f },
                          juce::Justification::centred, colours::dim);

        draw::engravedRule (g, { layout.gain.x - 52.0f, plate.getY() + 12.0f, (layout.tight.x + 36.0f) - (layout.gain.x - 52.0f), 12.0f },
                            "PREAMP", colours::dim);
        draw::engravedRule (g, { layout.bass.x - 36.0f, plate.getY() + 12.0f, (layout.presence.x + 36.0f) - (layout.bass.x - 36.0f), 12.0f },
                            "TONE", colours::dim);
        draw::engravedRule (g, { layout.master.x - 52.0f, plate.getY() + 12.0f, 104.0f, 12.0f }, "OUTPUT", colours::dim);

        faceplateKnob (g, layout.gain, layout.bigKnob, "GAIN", true);
        faceplateKnob (g, layout.tight, layout.knob, "TIGHT", false);
        faceplateKnob (g, layout.bass, layout.knob, "BASS", false);
        faceplateKnob (g, layout.mid, layout.knob, "MID", false);
        faceplateKnob (g, layout.treble, layout.knob, "TREBLE", false);
        faceplateKnob (g, layout.presence, layout.knob, "PRESENCE", false);
        faceplateKnob (g, layout.master, layout.bigKnob, "MASTER", true);

        // ---- pedalboard -----------------------------------------------------------
        draw::pedalboard (g, layout.board);
        paintDrop (g);
        paintSimplePedal (g, layout.gate, layout.gateTop, gatePaint, "GATE",
                          { { layout.gateThresh, "THRESH" }, { layout.gateHold, "HOLD" } });
        paintSimplePedal (g, layout.boost, layout.boostTop, boostPaint, "BOOST",
                          { { layout.boostDrive, "DRIVE" }, { layout.boostTone, "TONE" }, { layout.boostLevel, "LEVEL" } });
        paintSimplePedal (g, layout.shape, layout.shapeTop, shapePaint, "SHAPE",
                          { { layout.chug, "CHUG" }, { layout.chugFreq, "FREQ" }, { layout.dirt, "DIRT" } });
        paintVoid (g);
        paintCables (g, b);
    }

    void paintCabPlate (juce::Graphics& g)
    {
        const auto& cp = layout.cabPlate;
        draw::contactShadow (g, cp, 12.0f, 0.7f);
        draw::faceplate (g, cp);
        draw::displayGlass (g, layout.cabGlass, 5.0f);
        draw::silkscreen (g, "IMPULSE", Fonts::labelBold (10.0f, 0.3f),
                          { layout.cabNext.getRight() + 10.0f, layout.cabNext.getY(), 90.0f, layout.cabNext.getHeight() },
                          juce::Justification::centredLeft, colours::dim);

        draw::knobScale (g, layout.cabMix, layout.cabKnob * 0.5f + 5.0f, colours::bone.withAlpha (0.85f), false);
        draw::knobScale (g, layout.cabLowCut, layout.cabKnob * 0.5f + 5.0f, colours::bone.withAlpha (0.85f), false);
        for (auto [c, text] : { std::pair<juce::Point<float>, const char*> { layout.cabMix, "CAB MIX" }, { layout.cabLowCut, "LOW CUT" } })
            draw::silkscreen (g, text, Fonts::label (12.0f, 0.22f), { c.x - 60.0f, c.y + layout.cabKnob * 0.5f + 13.0f, 120.0f, 14.0f },
                              juce::Justification::centred, colours::bone);

        const float bx = layout.cabLowCut.x + 62.0f;
        draw::engravedRule (g, { bx, cp.getY() + 16.0f, cp.getRight() - 22.0f - bx, 10.0f }, {}, colours::dim);
        draw::chromeText (g, "APEX", Fonts::display (30.0f, 0.22f), { bx, cp.getY() + 28.0f, cp.getRight() - 22.0f - bx, 34.0f },
                          juce::Justification::centred);
        draw::silkscreen (g, "4x12 CABINET", Fonts::label (10.0f, 0.34f), { bx, cp.getY() + 64.0f, cp.getRight() - 22.0f - bx, 12.0f },
                          juce::Justification::centred, colours::dim);
    }

    void paintDrop (juce::Graphics& g)
    {
        const auto& t = layout.dropTop;
        draw::pedalEnclosure (g, layout.drop, dropPaint);
        draw::displayGlass (g, layout.dropGlass, 6.0f);
        pedalLabel (g, layout.dropBody, layout.dropKnob, "BODY", dropInk);
        pedalLabel (g, layout.dropSub, layout.dropKnob, "SUB", dropInk);
        pedalName (g, "DROP", { t.getX() + 58.0f, t.getY() + 104.0f, t.getWidth() - 116.0f, 30.0f }, dropInk, 28.0f);
    }

    void paintSimplePedal (juce::Graphics& g, juce::Rectangle<float> body, juce::Rectangle<float> t, juce::Colour paint,
                           const juce::String& name, std::initializer_list<std::pair<juce::Point<float>, const char*>> knobLabels)
    {
        draw::pedalEnclosure (g, body, paint);
        for (const auto& [c, text] : knobLabels)
            pedalLabel (g, c, layout.pedalKnob, text, colours::bone);
        pedalName (g, name, { t.getX(), t.getY() + 86.0f, t.getWidth(), 30.0f }, colours::bone, 25.0f);
    }

    void paintVoid (juce::Graphics& g)
    {
        const auto& t = layout.voidTop;
        draw::pedalEnclosure (g, layout.voidUnit, voidPaint);
        draw::displayGlass (g, layout.voidGlass, 5.0f);

        const float d = layout.voidKnob;
        for (auto [c, text] : { std::pair<juce::Point<float>, const char*> { layout.echoTime, "TIME" }, { layout.echoFeedback, "FDBK" },
                                { layout.echoDuck, "DUCK" }, { layout.echoMix, "MIX" }, { layout.abyssDecay, "DECAY" },
                                { layout.abyssDepth, "ABYSS" }, { layout.abyssTone, "TONE" }, { layout.abyssMix, "MIX" } })
            pedalLabel (g, c, d, text, colours::bone, 10.0f);

        const float ruleY = layout.echoTime.y + d * 0.5f + 9.0f;
        draw::engravedRule (g, { t.getX() + 12.0f, ruleY, layout.echoMix.x + d * 0.5f - t.getX() - 10.0f, 11.0f }, "ECHO", colours::bone);
        draw::engravedRule (g, { layout.abyssDecay.x - d * 0.5f - 2.0f, ruleY, t.getRight() - 12.0f - (layout.abyssDecay.x - d * 0.5f - 2.0f), 11.0f },
                            "ABYSS", colours::bone);

        pedalName (g, "VOID", { t.getCentreX() - 52.0f, t.getBottom() - 60.0f, 104.0f, 44.0f }, colours::bone, 38.0f);
        draw::silkscreen (g, "STEREO", Fonts::labelBold (9.0f, 0.4f), { t.getCentreX() - 52.0f, t.getBottom() - 20.0f, 104.0f, 10.0f },
                          juce::Justification::centred, colours::bone.withAlpha (0.6f));
    }

    void paintCables (juce::Graphics& g, juce::Rectangle<float> b)
    {
        const float plugW = 13.0f;
        auto plug = [&] (juce::Rectangle<float> pedal, bool input)
        {
            return draw::topJackPlug (g, { input ? pedal.getX() + 22.0f : pedal.getRight() - 22.0f, pedal.getY() + 2.0f }, plugW);
        };
        auto patch = [&] (juce::Point<float> a, juce::Point<float> c)
        {
            juce::Path p;
            p.startNewSubPath (a);
            p.cubicTo (a.x, a.y - 30.0f, c.x, c.y - 30.0f, c.x, c.y);
            draw::cable (g, p, 7.0f);
        };

        const juce::Rectangle<float> chain[] = { layout.drop, layout.gate, layout.boost, layout.shape, layout.voidUnit };
        const auto inPlug = plug (chain[0], true);
        for (size_t i = 0; i + 1 < std::size (chain); ++i)
        {
            const auto out = plug (chain[i], false);
            const auto in  = plug (chain[i + 1], true);
            patch (out, in);
        }
        const auto outPlug = plug (chain[std::size (chain) - 1], false);

        juce::Path p;
        p.startNewSubPath (inPlug);
        p.cubicTo (inPlug.x, inPlug.y - 40.0f, inPlug.x - 30.0f, inPlug.y - 50.0f, -10.0f, inPlug.y - 44.0f);
        draw::cable (g, p, 7.0f);
        juce::Path q;
        q.startNewSubPath (outPlug);
        q.cubicTo (outPlug.x, outPlug.y - 40.0f, outPlug.x + 30.0f, outPlug.y - 50.0f, b.getRight() + 10.0f, outPlug.y - 44.0f);
        draw::cable (g, q, 7.0f);
    }

    const Layout& layout;
};

//==============================================================================
struct AmpEditor::BlendTrims : public juce::Component
{
    BlendTrims()
    {
        for (auto* k : { &bite, &body, &edge })
            addAndMakeVisible (*k);
    }

    void setBlend (bool shouldShow)
    {
        if (blend == shouldShow)
            return;
        blend = shouldShow;
        for (auto* k : { &bite, &body, &edge })
            k->setVisible (blend);
        repaint();
    }

    void resized() override
    {
        const float w = (float) getWidth() / 3.0f;
        for (int i = 0; i < 3; ++i)
        {
            auto* k = std::array<Knob*, 3> { &bite, &body, &edge }[(size_t) i];
            k->setBounds (square ({ w * ((float) i + 0.5f), 17.0f }, 26.0f * Knob::boundsRatio).toNearestInt());
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (! blend)
        {
            draw::silkscreen (g, "RIG", Fonts::label (13.0f, 0.3f), getLocalBounds().toFloat().withTrimmedTop (22.0f).withHeight (16.0f),
                              juce::Justification::centred, colours::bone);
            return;
        }
        const float w = (float) getWidth() / 3.0f;
        const char* names[] = { "BITE", "BODY", "EDGE" };
        for (int i = 0; i < 3; ++i)
            draw::silkscreen (g, names[i], Fonts::label (10.0f, 0.18f), { w * (float) i, 34.0f, w, 12.0f },
                              juce::Justification::centred, colours::dim);
    }

    Knob bite { KnobStyle::spunAluminium }, body { KnobStyle::spunAluminium }, edge { KnobStyle::spunAluminium };
    bool blend = true;
};

//==============================================================================
AmpEditor::AmpEditor (ApexAmpProcessor& p)
    : EditorBase (p, p.apvts, p.presets, "Amp", designWidth, designHeight, &p.tunerFeed),
      proc (p),
      selector ({ "BITE", "BODY", "EDGE", "BLEND", "USER" }, -80.0f, 40.0f)
{
    ampStage = std::make_unique<AmpStage> (layout);
    stage.addAndMakeVisible (*ampStage);
    ampStage->setBounds (stage.getLocalBounds());
    auto& host = *ampStage;

    // ---- faceplate --------------------------------------------------------------
    makeKnob ("inputGain",  KnobStyle::spunAluminium, layout.gain,     layout.bigKnob, "Gain: drive into the amp");
    makeKnob ("tight",      KnobStyle::spunAluminium, layout.tight,    layout.knob, "Tight: low-cut before the amp for faster palm mutes");
    makeKnob ("bass",       KnobStyle::spunAluminium, layout.bass,     layout.knob, "Bass shelf, 110 Hz");
    makeKnob ("mid",        KnobStyle::spunAluminium, layout.mid,      layout.knob, "Mid, 700 Hz");
    makeKnob ("treble",     KnobStyle::spunAluminium, layout.treble,   layout.knob, "Treble shelf, 2.6 kHz");
    makeKnob ("presence",   KnobStyle::spunAluminium, layout.presence, layout.knob, "Presence: brightness after the cab, 3.5 kHz");
    makeKnob ("outputGain", KnobStyle::spunAluminium, layout.master,   layout.bigKnob, "Master output level");

    host.addAndMakeVisible (selector);
    selector.setBounds (layout.selector.toNearestInt());
    selector.setTooltip ("Rig: three captures of the same amp, a blend of all three, or your own .nam");
    selector.onChange = [this] (int position) { selectRig (position); };

    trims = std::make_unique<BlendTrims>();
    host.addAndMakeVisible (*trims);
    trims->setBounds (layout.trims.toNearestInt());
    const char* trimIds[] = { "mixBite", "mixBody", "mixEdge" };
    Knob* trimKnobs[] = { &trims->bite, &trims->body, &trims->edge };
    for (int i = 0; i < 3; ++i)
    {
        trimKnobs[i]->setPopupParent (&host);
        knobAttachments.push_back (attach (*trimKnobs[i], *apvts.getParameter (trimIds[i]), &proc.undoManager));
    }

    rigModeAttachment = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter ("rigMode"), [this] (float) { syncSelector(); }, &proc.undoManager);
    rigAttachment     = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter ("rig"),     [this] (float) { syncSelector(); }, &proc.undoManager);
    syncSelector();

    host.addAndMakeVisible (power);
    host.addAndMakeVisible (jewel);
    power.setBounds (layout.power.toNearestInt());
    jewel.setBounds (layout.jewel.toNearestInt());
    power.setTooltip ("Power (bypass)");
    bypassAttachment = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter ("bypass"), [this] (float v)
    {
        power.setToggleState (v < 0.5f, juce::dontSendNotification);
        jewel.setOn (v < 0.5f);
    }, &proc.undoManager);
    power.onClick = [this] { bypassAttachment->setValueAsCompleteGesture (power.getToggleState() ? 0.0f : 1.0f); };
    bypassAttachment->sendInitialUpdate();

    // ---- cabinet plate ----------------------------------------------------------------
    for (auto* c : std::initializer_list<juce::Component*> { &cabLcd, &cabPrev, &cabNext, &cabLoad })
        host.addAndMakeVisible (c);
    cabLcd.setBounds (layout.cabLcd.toNearestInt());
    cabPrev.setBounds (layout.cabPrev.toNearestInt());
    cabNext.setBounds (layout.cabNext.toNearestInt());
    cabLoad.setBounds (layout.cabLoad.toNearestInt());
    cabPrev.setTooltip ("Previous impulse response");
    cabNext.setTooltip ("Next impulse response");
    cabLoad.setTooltip ("Load your own impulse response (.wav / .aiff)");

    auto* ir = apvts.getParameter ("ir");
    irAttachment = std::make_unique<juce::ParameterAttachment> (*ir, [this, ir] (float v)
    {
        const int index = juce::roundToInt (v);
        const auto name = ir->getText (ir->convertTo0to1 ((float) index), 32).toUpperCase();
        cabLcd.setText (juce::String (index + 1).paddedLeft ('0', 2) + "  " + name,
                        index == 3 && ! proc.hasUserIr() ? "NO FILE LOADED" : "4x12  " + dot() + "  IR");
    }, &proc.undoManager);
    irAttachment->sendInitialUpdate();
    cabPrev.onClick = [this, ir] { stepParam (*irAttachment, *ir, -1, 0, proc.hasUserIr() ? 3 : 2); };
    cabNext.onClick = [this, ir] { stepParam (*irAttachment, *ir, 1, 0, proc.hasUserIr() ? 3 : 2); };
    cabLoad.onClick = [this] { loadIr(); };

    makeKnob ("cabMix", KnobStyle::spunAluminium, layout.cabMix, layout.cabKnob, "Cab mix: dry amp to full cabinet");
    makeKnob ("lowCut", KnobStyle::spunAluminium, layout.cabLowCut, layout.cabKnob, "Low cut after the cab");

    // ---- drop -------------------------------------------------------------------
    for (auto* c : std::initializer_list<juce::Component*> { &dropDigits, &dropCaption, &dropUp, &dropDown })
        host.addAndMakeVisible (c);
    dropDigits.setBounds (layout.dropDigits.toNearestInt());
    dropCaption.setBounds (layout.dropCaption.toNearestInt());
    dropUp.setBounds (layout.dropUp.toNearestInt());
    dropDown.setBounds (layout.dropDown.toNearestInt());
    dropUp.setTooltip ("Shift up a semitone");
    dropDown.setTooltip ("Shift down a semitone");

    auto* shift = apvts.getParameter ("dropShift");
    dropShiftAttachment = std::make_unique<juce::ParameterAttachment> (*shift, [this] (float v)
    {
        const int st = juce::roundToInt (v);
        dropDigits.setText (st > 0 ? "+" + juce::String (st) : juce::String (st));
        dropCaption.setText ("E STD " + juce::String::charToString (0x2192) + " " + noteName (4 + st)
                             + (st <= -12 ? " -1 OCT" : (st >= 12 ? " +1 OCT" : " STD")));
    }, &proc.undoManager);
    dropShiftAttachment->sendInitialUpdate();
    dropUp.onClick   = [this, shift] { stepParam (*dropShiftAttachment, *shift, 1, -12, 12); };
    dropDown.onClick = [this, shift] { stepParam (*dropShiftAttachment, *shift, -1, -12, 12); };

    makeKnob ("dropBody", KnobStyle::pedal, layout.dropBody, layout.dropKnob,
              "Body: keeps the guitar's pickup and body resonances where a real drop tuning keeps them");
    makeKnob ("dropSub", KnobStyle::pedal, layout.dropSub, layout.dropKnob, "Sub: adds an octave-down layer under the dropped guitar");
    makeFootswitch ("dropOn", dropLed, { layout.dropTop.getCentreX(), layout.dropTop.getBottom() - 32.0f }, "Drop on / off");

    // ---- gate / boost / shape ----------------------------------------------------------
    const float pk = layout.pedalKnob;
    makeKnob ("gate",     KnobStyle::pedal, layout.gateThresh, pk, "Gate threshold");
    makeKnob ("gateHold", KnobStyle::pedal, layout.gateHold, pk, "Gate hold: how long it stays open after the last note");
    makeFootswitch ("gateOn", gateLed, { layout.gateTop.getCentreX(), layout.gateTop.getBottom() - 32.0f }, "Gate on / off");

    makeKnob ("boostDrive", KnobStyle::pedal, layout.boostDrive, pk, "Boost drive");
    makeKnob ("boostTone",  KnobStyle::pedal, layout.boostTone, pk, "Boost tone");
    makeKnob ("boostLevel", KnobStyle::pedal, layout.boostLevel, pk, "Boost level");
    makeFootswitch ("boostOn", boostLed, { layout.boostTop.getCentreX(), layout.boostTop.getBottom() - 32.0f }, "Boost on / off");

    makeKnob ("chug",     KnobStyle::pedal, layout.chug, pk,
              "Chug: punch on every pick attack, read from the DI before the amp. Held notes stay untouched");
    makeKnob ("chugFreq", KnobStyle::pedal, layout.chugFreq, pk, "Chug frequency: low for thump, high for pick click");
    makeKnob ("dirt",     KnobStyle::pedal, layout.dirt, pk, "Low dirt: parallel growl on the low end, after the amp");
    makeFootswitch ("shapeOn", shapeLed, { layout.shapeTop.getCentreX(), layout.shapeTop.getBottom() - 32.0f }, "Shape on / off");

    // ---- void -------------------------------------------------------------------
    host.addAndMakeVisible (voidLcd);
    host.addAndMakeVisible (syncButton);
    host.addAndMakeVisible (tapButton);
    voidLcd.setBounds (layout.voidLcd.toNearestInt());
    syncButton.setBounds (layout.sync.toNearestInt());
    tapButton.setBounds (layout.tap.toNearestInt());
    syncButton.setTooltip ("Sync the echo to the host tempo");
    tapButton.setTooltip ("Tap the echo time");
    tapButton.setClickingTogglesState (false);
    tapButton.onClick = [this] { tapTempo(); };
    buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*apvts.getParameter ("delaySync"), syncButton, &proc.undoManager));

    const float vk = layout.voidKnob;
    echoTimeKnob = knobs.add (new Knob (KnobStyle::pedal));
    host.addAndMakeVisible (echoTimeKnob);
    echoTimeKnob->setBounds (square (layout.echoTime, vk * Knob::boundsRatio).toNearestInt());
    echoTimeKnob->setPopupParent (&host);
    syncAttachment = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter ("delaySync"), [this] (float) { bindEchoTime(); });
    bindEchoTime();

    makeKnob ("delayFeedback", KnobStyle::pedal, layout.echoFeedback, vk, "Echo feedback: how many repeats");
    makeKnob ("delayDuck",     KnobStyle::pedal, layout.echoDuck, vk, "Duck: keeps the repeats down while you play, lets them bloom in the gaps");
    makeKnob ("delayMix",      KnobStyle::pedal, layout.echoMix, vk, "Echo level");
    makeKnob ("reverbDecay",   KnobStyle::pedal, layout.abyssDecay, vk, "Reverb decay time");
    makeKnob ("reverbAbyss",   KnobStyle::pedal, layout.abyssDepth, vk,
              "Abyss: an octave-down shimmer. The tail sinks an octave on every pass");
    makeKnob ("reverbTone",    KnobStyle::pedal, layout.abyssTone, vk, "Reverb tone: dark to bright");
    makeKnob ("reverbMix",     KnobStyle::pedal, layout.abyssMix, vk, "Reverb level");
    makeFootswitch ("delayOn",  echoLed,  layout.echoSwitch,  "Echo on / off (repeats ring out when switched off)");
    makeFootswitch ("reverbOn", abyssLed, layout.abyssSwitch, "Abyss reverb on / off (the tail rings out when switched off)");

    // ---- Auto Input in the header ------------------------------------------------
    auto& header = getHeader();
    header.setShowsAutoInput (true);
    header.autoButton.onClick = [this]
    {
        if (! proc.isAutoInputListening())
            proc.startAutoInput();
        updateAutoButton();
    };

    tick();
}

AmpEditor::~AmpEditor() = default;

Knob& AmpEditor::makeKnob (const juce::String& paramId, KnobStyle style, juce::Point<float> centre, float diameter,
                           const juce::String& tip)
{
    auto* knob = knobs.add (new Knob (style));
    ampStage->addAndMakeVisible (knob);
    knob->setBounds (square (centre, diameter * Knob::boundsRatio).toNearestInt());
    knob->setPopupParent (ampStage.get());
    if (tip.isNotEmpty())
        knob->setTooltip (tip);
    knobAttachments.push_back (attach (*knob, *apvts.getParameter (paramId), &proc.undoManager));
    return *knob;
}

Footswitch& AmpEditor::makeFootswitch (const juce::String& paramId, Led& led, juce::Point<float> centre, const juce::String& tip)
{
    auto* fs = footswitches.add (new Footswitch());
    ampStage->addAndMakeVisible (fs);
    fs->setBounds (square (centre, 50.0f).toNearestInt());
    fs->setTooltip (tip);
    buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*apvts.getParameter (paramId), *fs, &proc.undoManager));

    ampStage->addAndMakeVisible (led);
    led.setBounds (square ({ centre.x, centre.y - 40.0f }, 36.0f).toNearestInt());
    return *fs;
}

void AmpEditor::bindEchoTime()
{
    const bool synced = apvts.getRawParameterValue ("delaySync")->load() > 0.5f;
    echoTimeAttachment.reset();
    echoTimeAttachment = attach (*echoTimeKnob, *apvts.getParameter (synced ? "delayDiv" : "delayTime"), &proc.undoManager);
    echoTimeKnob->setTooltip (synced ? "Echo time as a note value of the host tempo" : "Echo time in milliseconds");
}

void AmpEditor::setParam (const juce::String& paramId, float value)
{
    auto* param = apvts.getParameter (paramId);
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->convertTo0to1 (value));
    param->endChangeGesture();
}

void AmpEditor::tapTempo()
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

void AmpEditor::stepParam (juce::ParameterAttachment& attachment, juce::RangedAudioParameter& param,
                           int delta, int minValue, int maxValue)
{
    const int current = juce::roundToInt (param.convertFrom0to1 (param.getValue()));
    const int next = juce::jlimit (minValue, maxValue, current + delta);
    if (next != current)
    {
        proc.undoManager.beginNewTransaction();
        attachment.setValueAsCompleteGesture ((float) next);
    }
}

void AmpEditor::syncSelector()
{
    const bool blend = apvts.getRawParameterValue ("rigMode")->load() > 0.5f;
    const int rig = juce::roundToInt (apvts.getRawParameterValue ("rig")->load());
    const int position = blend ? 3 : (rig == 3 ? 4 : juce::jlimit (0, 2, rig));
    selector.setSelected (position, juce::dontSendNotification);
    if (trims != nullptr)
        trims->setBlend (blend);
}

void AmpEditor::selectRig (int position)
{
    if (position == 4 && ! proc.hasUserRig())
    {
        syncSelector();
        loadRig();
        return;
    }

    proc.undoManager.beginNewTransaction ("Rig");
    if (position == 3)
    {
        rigModeAttachment->setValueAsCompleteGesture (1.0f);
    }
    else
    {
        rigModeAttachment->setValueAsCompleteGesture (0.0f);
        rigAttachment->setValueAsCompleteGesture (position == 4 ? 3.0f : (float) position);
    }
    syncSelector();
}

void AmpEditor::loadRig()
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

void AmpEditor::loadIr()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a cabinet impulse response", juce::File(), "*.wav;*.aiff;*.aif;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile() && proc.loadUserIr (file))
                              {
                                  proc.undoManager.beginNewTransaction ("Load IR");
                                  irAttachment->setValueAsCompleteGesture (3.0f);
                              }
                          });
}

void AmpEditor::addSettingsItems (juce::PopupMenu& menu)
{
    menu.addSeparator();
    menu.addItem (100, "Load NAM rig" + juce::String::charToString (0x2026));
    menu.addItem (101, "Load cabinet IR" + juce::String::charToString (0x2026));
    menu.addSeparator();
    const float trim = apvts.getRawParameterValue ("inputTrim")->load();
    menu.addItem (102, "Auto Input (listen and set the input level)");
    menu.addItem (103, "Reset input trim (" + juce::String (trim, 1) + " dB)", std::abs (trim) > 0.05f);
}

void AmpEditor::handleSettingsItem (int id)
{
    if (id == 100) loadRig();
    if (id == 101) loadIr();
    if (id == 102) proc.startAutoInput();
    if (id == 103)
    {
        proc.undoManager.beginNewTransaction ("Reset input trim");
        setParam ("inputTrim", 0.0f);
    }
}

void AmpEditor::updateAutoButton()
{
    auto& button = getHeader().autoButton;
    const bool listening = proc.isAutoInputListening();
    const int outcome = proc.getAutoInputOutcome();

    if (outcome != lastAutoOutcome)
    {
        lastAutoOutcome = outcome;
        if (outcome == 1)
        {
            const float trim = proc.getAutoInputTrim();
            autoMessage = (trim >= 0.0f ? "+" : juce::String::charToString (0x2212)) + juce::String (std::abs (trim), 1);
            autoMessageFrames = 90;
        }
        else if (outcome == 2)
        {
            autoMessage = "NO SIG";
            autoMessageFrames = 90;
        }
    }

    juce::String text = "AUTO";
    if (listening)
        text = (++blink / 12) % 2 == 0 ? "PLAY" : "";
    else if (autoMessageFrames > 0)
    {
        --autoMessageFrames;
        text = autoMessage;
    }
    button.setButtonText (text);
    button.setToggleState (listening, juce::dontSendNotification);

    const float trim = apvts.getRawParameterValue ("inputTrim")->load();
    button.setTooltip (listening ? "Listening: play the heaviest part of your riff"
                                 : "Auto Input: play for a few seconds and the input level is set for you (trim now "
                                   + juce::String (trim, 1) + " dB)");
}

void AmpEditor::tick()
{
    auto on = [this] (const char* id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };
    dropLed.setOn (on ("dropOn"));
    gateLed.setOn (on ("gateOn"));
    boostLed.setOn (on ("boostOn"));
    shapeLed.setOn (on ("shapeOn"));
    echoLed.setOn (on ("delayOn"));
    abyssLed.setOn (on ("reverbOn"));
    selector.setItemEnabled (4, true);

    // Void display: echo time (and the division it follows), reverb decay.
    const bool synced = on ("delaySync");
    const int ms = juce::roundToInt (proc.getEchoMs());
    juce::String echo = juce::String (ms) + " MS";
    if (synced)
    {
        auto* div = apvts.getParameter ("delayDiv");
        echo = div->getCurrentValueAsText() + "  " + echo;
    }
    const float decay = apvts.getRawParameterValue ("reverbDecay")->load();
    voidLcd.setText (echo,
                     juce::String (synced ? "SYNC " + juce::String (juce::roundToInt (proc.getHostBpm())) + " BPM" : "FREE")
                         + "   ABYSS " + juce::String (decay, decay < 10.0f ? 1 : 0) + " S");

    updateAutoButton();
}

float AmpEditor::getInputPeak()  { return proc.inputMagnitude.load(); }
float AmpEditor::getOutputPeak() { return proc.outputMagnitude.load(); }
