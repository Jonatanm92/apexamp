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
    const juce::Colour cabPaint   { 0xff1c1c1f };
    const juce::Colour dropInk    { 0xff1c0e06 };
    const juce::Colour redLed     { 0xffff3b1f };

    juce::Rectangle<float> square (juce::Point<float> c, float d) { return juce::Rectangle<float> (d, d).withCentre (c); }

    juce::String noteName (int semitone)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[((semitone % 12) + 12) % 12];
    }
}

//==============================================================================
AmpEditor::Layout::Layout()
{
    head  = { 40.0f, 8.0f, 1120.0f, 292.0f };
    plate = draw::ampHeadFaceplate (head);
    cab   = { 22.0f, 280.0f, 1156.0f, 520.0f };
    board = { 40.0f, 448.0f, 1120.0f, 250.0f };

    drop    = { 64.0f, 466.0f, 274.0f, 212.0f };
    gate    = { 364.0f, 466.0f, 196.0f, 212.0f };
    boost   = { 586.0f, 466.0f, 236.0f, 212.0f };
    cabUnit = { 848.0f, 466.0f, 288.0f, 212.0f };
    dropTop  = draw::pedalTopFace (drop);
    gateTop  = draw::pedalTopFace (gate);
    boostTop = draw::pedalTopFace (boost);
    cabTop   = draw::pedalTopFace (cabUnit);

    // faceplate
    const float ky = plate.getCentreY() - 8.0f;
    float x = plate.getX() + 410.0f;
    gain     = { x, ky }; x += 106.0f;
    tight    = { x, ky }; x += 86.0f;
    bass     = { x, ky }; x += 86.0f;
    mid      = { x, ky }; x += 86.0f;
    treble   = { x, ky }; x += 86.0f;
    presence = { x, ky }; x += 106.0f;
    master   = { x, ky };

    const juce::Point<float> selCentre (plate.getX() + 262.0f, plate.getCentreY() + 4.0f);
    selector = { selCentre.x - 86.0f, selCentre.y - 93.0f, 172.0f, 150.0f };
    trims    = { selCentre.x - 80.0f, plate.getBottom() - 56.0f, 160.0f, 50.0f };
    power    = { plate.getX() + 96.0f, plate.getBottom() - 86.0f, 30.0f, 70.0f };
    jewel    = square ({ plate.getX() + 45.0f, plate.getBottom() - 51.0f }, 80.0f);

    // drop pedal
    dropGlass   = { dropTop.getX() + 14.0f, dropTop.getY() + 14.0f, 128.0f, 76.0f };
    dropDigits  = dropGlass.reduced (14.0f, 8.0f).withTrimmedBottom (18.0f);
    dropCaption = { dropGlass.getX() + 4.0f, dropGlass.getBottom() - 22.0f, dropGlass.getWidth() - 8.0f, 18.0f };
    dropUp      = { dropGlass.getRight() + 6.0f, dropGlass.getY(), 26.0f, 35.0f };
    dropDown    = { dropGlass.getRight() + 6.0f, dropGlass.getY() + 41.0f, 26.0f, 35.0f };
    dropBody    = { dropTop.getRight() - 42.0f, dropTop.getY() + 52.0f };
    dropSub     = { dropTop.getX() + 40.0f, dropTop.getY() + 130.0f };

    gateThresh = { gateTop.getX() + 44.0f, gateTop.getY() + 48.0f };
    gateHold   = { gateTop.getRight() - 44.0f, gateTop.getY() + 48.0f };

    boostDrive = { boostTop.getX() + 42.0f, boostTop.getY() + 48.0f };
    boostTone  = { boostTop.getCentreX(), boostTop.getY() + 48.0f };
    boostLevel = { boostTop.getRight() - 42.0f, boostTop.getY() + 48.0f };

    cabGlass  = { cabTop.getX() + 14.0f, cabTop.getY() + 14.0f, cabTop.getWidth() - 28.0f, 44.0f };
    cabLcd    = cabGlass.reduced (12.0f, 3.0f);
    cabPrev   = { cabTop.getX() + 14.0f, cabTop.getY() + 66.0f, 32.0f, 26.0f };
    cabNext   = { cabTop.getX() + 50.0f, cabTop.getY() + 66.0f, 32.0f, 26.0f };
    cabLoad   = { cabTop.getRight() - 46.0f, cabTop.getY() + 66.0f, 32.0f, 26.0f };
    cabMix    = { cabTop.getX() + 150.0f, cabTop.getY() + 136.0f };
    cabLowCut = { cabTop.getRight() - 44.0f, cabTop.getY() + 136.0f };
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

    void pedalLabel (juce::Graphics& g, juce::Point<float> knobCentre, float d, const juce::String& text, juce::Colour ink)
    {
        draw::silkscreen (g, text, Fonts::labelBold (11.0f, 0.2f), { knobCentre.x - 40.0f, knobCentre.y - d * 0.5f - 18.0f, 80.0f, 12.0f },
                          juce::Justification::centred, ink);
    }

    void pedalName (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, juce::Justification j, juce::Colour ink, float size = 26.0f)
    {
        draw::silkscreen (g, name, Fonts::display (size, 0.2f), area, j, ink);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        draw::stage (g, b, { b.getCentreX(), 60.0f });

        // ---- 4x12 behind, head on top ------------------------------------------
        {
            const juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (getLocalBounds());
            const auto grille = draw::speakerCab (g, layout.cab);
            draw::chromeText (g, "APEX", Fonts::display (28.0f, 0.22f),
                              { grille.getCentreX() - 70.0f, grille.getY() + 30.0f, 140.0f, 32.0f }, juce::Justification::centred);
        }

        const auto plate = draw::ampHead (g, layout.head);
        draw::faceplate (g, plate);
        draw::chromeText (g, "APEX", Fonts::display (56.0f, 0.16f), { plate.getX() + 26.0f, plate.getY() + 26.0f, 190.0f, 66.0f },
                          juce::Justification::centredLeft);
        draw::silkscreen (g, "NEURAL HIGH-GAIN", Fonts::label (11.0f, 0.34f), { plate.getX() + 30.0f, plate.getY() + 94.0f, 190.0f, 14.0f },
                          juce::Justification::centredLeft, colours::dim);
        draw::silkscreen (g, "POWER", Fonts::label (11.0f, 0.3f), { layout.power.getCentreX() - 40.0f, plate.getBottom() - 22.0f, 80.0f, 14.0f },
                          juce::Justification::centred, colours::dim);

        // group captions
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

        // ---- pedalboard ---------------------------------------------------------
        draw::pedalboard (g, layout.board);

        draw::pedalEnclosure (g, layout.drop, dropPaint);
        draw::displayGlass (g, layout.dropGlass, 6.0f);
        pedalLabel (g, layout.dropBody, 48.0f, "BODY", dropInk);
        pedalLabel (g, layout.dropSub, 34.0f, "SUB", dropInk);
        pedalName (g, "DROP", { layout.dropTop.getRight() - 120.0f, layout.dropTop.getY() + 100.0f, 108.0f, 32.0f },
                   juce::Justification::centredRight, dropInk, 30.0f);
        draw::silkscreen (g, "LIVE PITCH", Fonts::labelBold (10.0f, 0.3f), { layout.dropTop.getRight() - 120.0f, layout.dropTop.getY() + 130.0f, 108.0f, 12.0f },
                          juce::Justification::centredRight, dropInk.withAlpha (0.8f));

        draw::pedalEnclosure (g, layout.gate, gatePaint);
        pedalLabel (g, layout.gateThresh, 44.0f, "THRESH", colours::bone);
        pedalLabel (g, layout.gateHold, 44.0f, "HOLD", colours::bone);
        pedalName (g, "GATE", { layout.gateTop.getX(), layout.gateTop.getY() + 84.0f, layout.gateTop.getWidth(), 30.0f },
                   juce::Justification::centred, colours::bone);

        draw::pedalEnclosure (g, layout.boost, boostPaint);
        pedalLabel (g, layout.boostDrive, 42.0f, "DRIVE", colours::bone);
        pedalLabel (g, layout.boostTone, 42.0f, "TONE", colours::bone);
        pedalLabel (g, layout.boostLevel, 42.0f, "LEVEL", colours::bone);
        pedalName (g, "BOOST", { layout.boostTop.getX(), layout.boostTop.getY() + 84.0f, layout.boostTop.getWidth(), 30.0f },
                   juce::Justification::centred, colours::bone);

        draw::pedalEnclosure (g, layout.cabUnit, cabPaint);
        draw::displayGlass (g, layout.cabGlass, 5.0f);
        pedalLabel (g, layout.cabMix, 42.0f, "MIX", colours::bone);
        pedalLabel (g, layout.cabLowCut, 42.0f, "LOW CUT", colours::bone);
        draw::silkscreen (g, "IMPULSE", Fonts::labelBold (10.0f, 0.3f), { layout.cabNext.getRight() + 8.0f, layout.cabNext.getY(), 80.0f, layout.cabNext.getHeight() },
                          juce::Justification::centredLeft, colours::dim);
        pedalName (g, "CAB", { layout.cabTop.getX() + 16.0f, layout.cabTop.getBottom() - 44.0f, 100.0f, 30.0f },
                   juce::Justification::centredLeft, colours::bone);

        // ---- patch cables (top-mounted jacks) ----------------------------------
        const float plugW = 13.0f;
        auto plug = [&] (juce::Rectangle<float> pedal, bool input)
        {
            return draw::topJackPlug (g, { input ? pedal.getX() + 24.0f : pedal.getRight() - 24.0f, pedal.getY() + 2.0f }, plugW);
        };
        auto patch = [&] (juce::Point<float> a, juce::Point<float> c)
        {
            juce::Path p;
            p.startNewSubPath (a);
            p.cubicTo (a.x, a.y - 34.0f, c.x, c.y - 34.0f, c.x, c.y);
            draw::cable (g, p, 7.0f);
        };

        const auto inPlug  = plug (layout.drop, true);
        const auto d1 = plug (layout.drop, false), g0 = plug (layout.gate, true);
        const auto g1 = plug (layout.gate, false), b0 = plug (layout.boost, true);
        const auto b1 = plug (layout.boost, false), c0 = plug (layout.cabUnit, true);
        const auto outPlug = plug (layout.cabUnit, false);
        patch (d1, g0);
        patch (g1, b0);
        patch (b1, c0);
        {
            juce::Path p;
            p.startNewSubPath (inPlug);
            p.cubicTo (inPlug.x, inPlug.y - 40.0f, inPlug.x - 30.0f, inPlug.y - 50.0f, -10.0f, inPlug.y - 44.0f);
            draw::cable (g, p, 7.0f);
            juce::Path q;
            q.startNewSubPath (outPlug);
            q.cubicTo (outPlug.x, outPlug.y - 40.0f, outPlug.x + 30.0f, outPlug.y - 50.0f, b.getRight() + 10.0f, outPlug.y - 44.0f);
            draw::cable (g, q, 7.0f);
        }
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
    makeKnob ("inputGain",  KnobStyle::spunAluminium, layout.gain,     layout.bigKnob).setTooltip ("Gain: drive into the amp");
    makeKnob ("tight",      KnobStyle::spunAluminium, layout.tight,    layout.knob).setTooltip ("Tight: low-cut before the amp for faster palm mutes");
    makeKnob ("bass",       KnobStyle::spunAluminium, layout.bass,     layout.knob).setTooltip ("Bass shelf, 110 Hz");
    makeKnob ("mid",        KnobStyle::spunAluminium, layout.mid,      layout.knob).setTooltip ("Mid, 700 Hz");
    makeKnob ("treble",     KnobStyle::spunAluminium, layout.treble,   layout.knob).setTooltip ("Treble shelf, 2.6 kHz");
    makeKnob ("presence",   KnobStyle::spunAluminium, layout.presence, layout.knob).setTooltip ("Presence: brightness after the cab, 3.5 kHz");
    makeKnob ("outputGain", KnobStyle::spunAluminium, layout.master,   layout.bigKnob).setTooltip ("Master output level");

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

    auto* rigMode = apvts.getParameter ("rigMode");
    auto* rig     = apvts.getParameter ("rig");
    rigModeAttachment = std::make_unique<juce::ParameterAttachment> (*rigMode, [this] (float) { syncSelector(); }, &proc.undoManager);
    rigAttachment     = std::make_unique<juce::ParameterAttachment> (*rig,     [this] (float) { syncSelector(); }, &proc.undoManager);
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

    // ---- drop -------------------------------------------------------------------
    host.addAndMakeVisible (dropDigits);
    host.addAndMakeVisible (dropCaption);
    host.addAndMakeVisible (dropUp);
    host.addAndMakeVisible (dropDown);
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

    makeKnob ("dropBody", KnobStyle::pedal, layout.dropBody, 48.0f)
        .setTooltip ("Body: keeps the guitar's pickup and body resonances where a real drop tuning keeps them");
    makeKnob ("dropSub", KnobStyle::pedal, layout.dropSub, 34.0f).setTooltip ("Sub: adds an octave-down layer under the dropped guitar");
    makeFootswitch ("dropOn", dropLed, layout.dropTop).setTooltip ("Drop on / off");

    // ---- gate / boost --------------------------------------------------------------
    makeKnob ("gate",     KnobStyle::pedal, layout.gateThresh, 44.0f);
    makeKnob ("gateHold", KnobStyle::pedal, layout.gateHold, 44.0f);
    makeFootswitch ("gateOn", gateLed, layout.gateTop).setTooltip ("Gate on / off");
    makeKnob ("boostDrive", KnobStyle::pedal, layout.boostDrive, 42.0f);
    makeKnob ("boostTone",  KnobStyle::pedal, layout.boostTone, 42.0f);
    makeKnob ("boostLevel", KnobStyle::pedal, layout.boostLevel, 42.0f);
    makeFootswitch ("boostOn", boostLed, layout.boostTop).setTooltip ("Boost on / off");

    // ---- cab ----------------------------------------------------------------------
    host.addAndMakeVisible (cabLcd);
    host.addAndMakeVisible (cabPrev);
    host.addAndMakeVisible (cabNext);
    host.addAndMakeVisible (cabLoad);
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
                        index == 3 && ! proc.hasUserIr() ? "NO FILE LOADED" : "4x12  " + juce::String::charToString (0x00B7) + "  IR");
    }, &proc.undoManager);
    irAttachment->sendInitialUpdate();
    cabPrev.onClick = [this, ir] { stepParam (*irAttachment, *ir, -1, 0, proc.hasUserIr() ? 3 : 2); };
    cabNext.onClick = [this, ir] { stepParam (*irAttachment, *ir, 1, 0, proc.hasUserIr() ? 3 : 2); };
    cabLoad.onClick = [this] { loadIr(); };

    makeKnob ("cabMix", KnobStyle::pedal, layout.cabMix, 42.0f).setTooltip ("Cab mix: dry amp to full cabinet");
    makeKnob ("lowCut", KnobStyle::pedal, layout.cabLowCut, 42.0f).setTooltip ("Low cut after the cab");

    tick();
}

AmpEditor::~AmpEditor() = default;

Knob& AmpEditor::makeKnob (const juce::String& paramId, KnobStyle style, juce::Point<float> centre, float diameter)
{
    auto* knob = knobs.add (new Knob (style));
    ampStage->addAndMakeVisible (knob);
    knob->setBounds (square (centre, diameter * Knob::boundsRatio).toNearestInt());
    knob->setPopupParent (ampStage.get());
    knobAttachments.push_back (attach (*knob, *apvts.getParameter (paramId), &proc.undoManager));
    return *knob;
}

Footswitch& AmpEditor::makeFootswitch (const juce::String& paramId, Led& led, juce::Rectangle<float> top)
{
    auto* fs = footswitches.add (new Footswitch());
    ampStage->addAndMakeVisible (fs);
    fs->setBounds (square ({ top.getCentreX(), top.getBottom() - 32.0f }, 50.0f).toNearestInt());
    buttonAttachments.push_back (std::make_unique<juce::ButtonParameterAttachment> (*apvts.getParameter (paramId), *fs, &proc.undoManager));

    ampStage->addAndMakeVisible (led);
    led.setBounds (square ({ top.getCentreX(), top.getBottom() - 72.0f }, 36.0f).toNearestInt());
    return *fs;
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
}

void AmpEditor::handleSettingsItem (int id)
{
    if (id == 100) loadRig();
    if (id == 101) loadIr();
}

void AmpEditor::resized()
{
    EditorBase::resized();
}

void AmpEditor::tick()
{
    dropLed.setOn (apvts.getRawParameterValue ("dropOn")->load() > 0.5f);
    gateLed.setOn (apvts.getRawParameterValue ("gateOn")->load() > 0.5f);
    boostLed.setOn (apvts.getRawParameterValue ("boostOn")->load() > 0.5f);
    selector.setItemEnabled (4, true);
}

float AmpEditor::getInputPeak()  { return proc.inputMagnitude.load(); }
float AmpEditor::getOutputPeak() { return proc.outputMagnitude.load(); }
