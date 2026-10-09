#include "DropEditor.h"
#include "apex/ui/Objects.h"
#include "apex/ui/Theme.h"

using namespace apex::ui;

namespace
{
    constexpr int designWidth = 820, designHeight = 540;

    const juce::Colour pedalPaint { 0xffd2611f };
    const juce::Colour ink   { 0xff1c0e06 };

    // Stage layout (design coordinates under the header).
    const juce::Rectangle<float> unit    { 70.0f, 58.0f, 680.0f, 416.0f };
    const juce::Rectangle<float> top     = draw::pedalTopFace (unit);
    const juce::Rectangle<float> glass   { top.getX() + 28.0f, top.getY() + 26.0f, 330.0f, 132.0f };
    const juce::Rectangle<float> digitsArea { glass.getX() + 26.0f, glass.getY() + 14.0f, glass.getWidth() - 52.0f, 82.0f };
    const juce::Rectangle<float> captionArea { glass.getX() + 8.0f, glass.getBottom() - 32.0f, glass.getWidth() - 16.0f, 22.0f };
    const juce::Rectangle<float> upArea   { glass.getRight() + 10.0f, glass.getY(), 38.0f, 62.0f };
    const juce::Rectangle<float> downArea { glass.getRight() + 10.0f, glass.getY() + 70.0f, 38.0f, 62.0f };

    const juce::Point<float> bodyKnob   { top.getRight() - 118.0f, top.getY() + 92.0f };
    const float bodyD = 92.0f;
    const float rowY = top.getY() + 230.0f;
    const juce::Point<float> fineKnob   { top.getX() + 70.0f,  rowY };
    const juce::Point<float> subKnob    { top.getX() + 176.0f, rowY };
    const juce::Point<float> mixKnob    { top.getX() + 282.0f, rowY };
    const juce::Point<float> outKnob    { top.getX() + 388.0f, rowY };
    const float rowD = 52.0f;
    const juce::Rectangle<float> toggleArea { top.getRight() - 136.0f, rowY - 40.0f, 32.0f, 80.0f };

    juce::Rectangle<float> square (juce::Point<float> c, float d) { return juce::Rectangle<float> (d, d).withCentre (c); }

    juce::String noteName (int semitone)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[((semitone % 12) + 12) % 12];
    }
}

//==============================================================================
struct DropEditor::DropStage : public juce::Component
{
    DropStage()
    {
        setOpaque (true);
        setBufferedToImage (true);
        setInterceptsMouseClicks (false, true);
    }

    void label (juce::Graphics& g, juce::Point<float> c, float d, const juce::String& text)
    {
        draw::silkscreen (g, text, Fonts::labelBold (12.0f, 0.24f), { c.x - 50.0f, c.y - d * 0.5f - 21.0f, 100.0f, 14.0f },
                          juce::Justification::centred, ink);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        draw::stage (g, b, { b.getCentreX(), 40.0f });

        // floor under the unit
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0.0f, b.getBottom() - 120.0f,
                                                 juce::Colour (0xff0f0f11), 0.0f, b.getBottom(), false));
        g.fillRect (b.withTrimmedTop (b.getHeight() - 120.0f));

        draw::pedalEnclosure (g, unit, pedalPaint);
        draw::displayGlass (g, glass, 8.0f);

        label (g, bodyKnob, bodyD, "BODY");
        draw::knobScale (g, bodyKnob, bodyD * 0.5f + 7.0f, ink.withAlpha (0.85f), false);
        label (g, fineKnob, rowD, "FINE");
        label (g, subKnob, rowD, "SUB");
        label (g, mixKnob, rowD, "MIX");
        label (g, outKnob, rowD, "OUTPUT");

        draw::silkscreen (g, "LIVE", Fonts::labelBold (12.0f, 0.24f), toggleArea.withY (toggleArea.getY() - 22.0f).withHeight (14.0f).expanded (30.0f, 0.0f),
                          juce::Justification::centred, ink);
        draw::silkscreen (g, "STUDIO", Fonts::labelBold (12.0f, 0.24f), toggleArea.withY (toggleArea.getBottom() + 8.0f).withHeight (14.0f).expanded (30.0f, 0.0f),
                          juce::Justification::centred, ink);

        // brand + name
        draw::silkscreen (g, "APEX", Fonts::display (22.0f, 0.3f), { top.getX() + 30.0f, top.getBottom() - 56.0f, 120.0f, 28.0f },
                          juce::Justification::centredLeft, ink);
        draw::silkscreen (g, "FORMANT-CORRECT PITCH", Fonts::labelBold (10.0f, 0.3f), { top.getX() + 30.0f, top.getBottom() - 30.0f, 220.0f, 12.0f },
                          juce::Justification::centredLeft, ink.withAlpha (0.8f));
        draw::silkscreen (g, "DROP", Fonts::display (52.0f, 0.16f), { top.getRight() - 230.0f, top.getBottom() - 74.0f, 200.0f, 56.0f },
                          juce::Justification::centredRight, ink);

        // jacks on the top edge with cables leaving the frame
        const float plugW = 15.0f;
        for (auto [x, toRight] : { std::pair<float, bool> { unit.getX() + 40.0f, false }, { unit.getRight() - 40.0f, true } })
        {
            const auto end = draw::topJackPlug (g, { x, unit.getY() + 2.0f }, plugW);
            juce::Path p;
            p.startNewSubPath (end);
            p.cubicTo (end.x, end.y - 22.0f, end.x + (toRight ? 30.0f : -30.0f), end.y - 26.0f,
                       toRight ? b.getRight() + 10.0f : -10.0f, end.y - 14.0f);
            draw::cable (g, p, 8.0f);
        }
    }
};

//==============================================================================
DropEditor::DropEditor (DropProcessor& p)
    : EditorBase (p, p.apvts, p.presets, "Drop", designWidth, designHeight, nullptr),
      proc (p)
{
    dropStage = std::make_unique<DropStage>();
    stage.addAndMakeVisible (*dropStage);
    dropStage->setBounds (stage.getLocalBounds());
    auto& host = *dropStage;

    host.addAndMakeVisible (digits);
    host.addAndMakeVisible (caption);
    host.addAndMakeVisible (up);
    host.addAndMakeVisible (down);
    digits.setBounds (digitsArea.toNearestInt());
    caption.setBounds (captionArea.toNearestInt());
    up.setBounds (upArea.toNearestInt());
    down.setBounds (downArea.toNearestInt());
    up.setTooltip ("Up a semitone");
    down.setTooltip ("Down a semitone");
    up.onClick   = [this] { stepShift (1); };
    down.onClick = [this] { stepShift (-1); };

    shiftAttachment = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter ("shift"), [this] (float) { tick(); }, &proc.undoManager);

    makeKnob ("body",   bodyKnob, bodyD, "Body: keeps the guitar's pickup and body resonances where a real drop tuning keeps them");
    makeKnob ("fine",   fineKnob, rowD, "Fine tune in cents");
    makeKnob ("sub",    subKnob,  rowD, "Sub: an octave-down layer under the shifted signal");
    makeKnob ("mix",    mixKnob,  rowD, "Dry / shifted blend (dry is latency-aligned)");
    makeKnob ("output", outKnob,  rowD, "Output level");

    host.addAndMakeVisible (modeToggle);
    modeToggle.setBounds (toggleArea.toNearestInt());
    modeToggle.setTooltip ("Live: low latency for playing through it.  Studio: highest quality, latency compensated by your DAW");
    modeAttachment = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter ("mode"), [this] (float v)
    {
        modeToggle.setToggleState (v < 0.5f, juce::dontSendNotification);
        tick();
    }, &proc.undoManager);
    modeToggle.onClick = [this] { modeAttachment->setValueAsCompleteGesture (modeToggle.getToggleState() ? 0.0f : 1.0f); };
    modeAttachment->sendInitialUpdate();

    host.addAndMakeVisible (stomp);
    host.addAndMakeVisible (led);
    stomp.setBounds (square ({ top.getCentreX(), top.getBottom() - 52.0f }, 64.0f).toNearestInt());
    led.setBounds (square ({ top.getCentreX(), top.getBottom() - 104.0f }, 44.0f).toNearestInt());
    stomp.setTooltip ("On / bypass");
    bypassAttachment = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter ("bypass"), [this] (float v)
    {
        stomp.setToggleState (v < 0.5f, juce::dontSendNotification);
        led.setOn (v < 0.5f);
    }, &proc.undoManager);
    stomp.onClick = [this] { bypassAttachment->setValueAsCompleteGesture (stomp.getToggleState() ? 0.0f : 1.0f); };
    bypassAttachment->sendInitialUpdate();

    tick();
}

DropEditor::~DropEditor() = default;

Knob& DropEditor::makeKnob (const juce::String& paramId, juce::Point<float> centre, float diameter, const juce::String& tip)
{
    auto* knob = knobs.add (new Knob (KnobStyle::pedal));
    dropStage->addAndMakeVisible (knob);
    knob->setBounds (square (centre, diameter * Knob::boundsRatio).toNearestInt());
    knob->setPopupParent (dropStage.get());
    knob->setTooltip (tip);
    knobAttachments.push_back (attach (*knob, *apvts.getParameter (paramId), &proc.undoManager));
    return *knob;
}

void DropEditor::stepShift (int delta)
{
    auto* p = apvts.getParameter ("shift");
    const int current = juce::roundToInt (p->convertFrom0to1 (p->getValue()));
    const int next = juce::jlimit (-24, 24, current + delta);
    if (next != current)
    {
        proc.undoManager.beginNewTransaction();
        shiftAttachment->setValueAsCompleteGesture ((float) next);
    }
}

void DropEditor::tick()
{
    const int st = juce::roundToInt (apvts.getRawParameterValue ("shift")->load());
    digits.setText (st > 0 ? "+" + juce::String (st) : juce::String (st));

    const bool live = apvts.getRawParameterValue ("mode")->load() < 0.5f;
    const auto mode = live ? apex::dsp::PitchMode::live : apex::dsp::PitchMode::studio;
    const int octaves = st / 12;
    const juce::String target = st == 0 ? "E STD"
                              : noteName (4 + st) + " STD" + (octaves != 0 ? (octaves < 0 ? " -" : " +") + juce::String (std::abs (octaves)) + " OCT" : juce::String());
    caption.setText ("E STD " + juce::String::charToString (0x2192) + " " + target + "   "
                     + (live ? "LIVE " : "STUDIO ") + juce::String (juce::roundToInt (proc.getLatencyMs (mode))) + " MS");
}
