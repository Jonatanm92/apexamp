// Renders apex-ui materials and a static composition prototype to PNG for review.
#include <juce_gui_basics/juce_gui_basics.h>
#include "apex/ui/Materials.h"
#include "apex/ui/Objects.h"
#include "apex/ui/Theme.h"

using namespace apex::ui;

struct Composition : juce::Component
{
    Composition() { setSize (1200, 760); }

    void ampKnob (juce::Graphics& g, juce::Point<float> c, float d, float v, const juce::String& label)
    {
        const auto area = juce::Rectangle<float> (d, d).withCentre (c);
        draw::knobScale (g, c, d * 0.5f + 5.0f, colours::bone.withAlpha (0.85f), d > 70.0f);
        draw::knob (g, area, -2.356f + v * 4.712f, KnobStyle::spunAluminium);
        draw::silkscreen (g, label, Fonts::label (d > 70.0f ? 15.0f : 13.0f, 0.22f),
                          { c.x - 60.0f, c.y + d * 0.5f + (d > 70.0f ? 28.0f : 18.0f), 120.0f, 16.0f },
                          juce::Justification::centred, colours::bone);
    }

    void pedalKnob (juce::Graphics& g, juce::Point<float> c, float d, float v, const juce::String& label, juce::Colour ink)
    {
        draw::silkscreen (g, label, Fonts::labelBold (11.0f, 0.2f), { c.x - 40.0f, c.y - d * 0.5f - 17.0f, 80.0f, 12.0f },
                          juce::Justification::centred, ink);
        draw::knob (g, juce::Rectangle<float> (d, d).withCentre (c), -2.356f + v * 4.712f, KnobStyle::pedal);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        // header
        auto header = b.removeFromTop (48.0f);
        g.setColour (colours::headerBg);
        g.fillRect (header);
        g.setColour (colours::hairline);
        g.drawLine (0.0f, 47.5f, b.getWidth(), 47.5f);
        g.setColour (colours::bone);
        g.setFont (Fonts::display (24.0f, 0.24f));
        g.drawText ("APEX", header.withTrimmedLeft (20.0f).withWidth (100.0f), juce::Justification::centredLeft);

        draw::stage (g, b, { b.getCentreX(), b.getY() + 100.0f });

        // cab behind, head on top
        {
            const juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (b.toNearestInt());
            const auto grille = draw::speakerCab (g, { 22.0f, 330.0f, 1156.0f, 520.0f });
            draw::chromeText (g, "APEX", Fonts::display (30.0f, 0.2f), { grille.getCentreX() - 70.0f, grille.getY() + 18.0f, 140.0f, 34.0f },
                              juce::Justification::centred);
        }
        const auto head = juce::Rectangle<float> (40.0f, 58.0f, 1120.0f, 292.0f);
        const auto plate = draw::ampHead (g, head);
        draw::faceplate (g, plate);

        draw::chromeText (g, "APEX", Fonts::display (56.0f, 0.16f), { plate.getX() + 26.0f, plate.getY() + 30.0f, 190.0f, 66.0f },
                          juce::Justification::centredLeft);
        draw::silkscreen (g, "NEURAL HIGH-GAIN", Fonts::label (11.0f, 0.34f), { plate.getX() + 30.0f, plate.getY() + 98.0f, 190.0f, 14.0f },
                          juce::Justification::centredLeft, colours::dim);
        draw::jewel (g, { plate.getX() + 34.0f, plate.getBottom() - 62.0f, 22.0f, 22.0f }, true, colours::amber);
        draw::batToggle (g, { plate.getX() + 96.0f, plate.getBottom() - 86.0f, 30.0f, 70.0f }, true);
        draw::silkscreen (g, "POWER", Fonts::label (11.0f, 0.3f), { plate.getX() + 70.0f, plate.getBottom() - 26.0f, 80.0f, 14.0f },
                          juce::Justification::centred, colours::dim);

        // rig selector
        const juce::Point<float> sel (plate.getX() + 262.0f, plate.getCentreY() + 4.0f);
        const char* rigs[] = { "BITE", "BODY", "EDGE", "BLEND", "USER" };
        for (int i = 0; i < 5; ++i)
        {
            const float a = (-80.0f + 40.0f * (float) i) * juce::MathConstants<float>::pi / 180.0f;
            const juce::Point<float> p (sel.x + std::sin (a) * 68.0f, sel.y - std::cos (a) * 68.0f);
            g.setColour (i == 0 ? colours::amber : colours::faint);
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ sel.x + std::sin (a) * 49.0f, sel.y - std::cos (a) * 49.0f }));
            draw::silkscreen (g, rigs[i], Fonts::label (12.0f, 0.16f), juce::Rectangle<float> (60.0f, 14.0f).withCentre (p),
                              juce::Justification::centred, i == 0 ? colours::bone : colours::dim);
        }
        draw::knob (g, juce::Rectangle<float> (76.0f, 76.0f).withCentre (sel), -1.396f, KnobStyle::chickenHead);
        draw::silkscreen (g, "RIG", Fonts::label (13.0f, 0.3f), { sel.x - 40.0f, plate.getBottom() - 30.0f, 80.0f, 14.0f },
                          juce::Justification::centred, colours::bone);

        // knob row
        const float ky = plate.getCentreY() - 8.0f;
        float x = plate.getX() + 418.0f;
        ampKnob (g, { x, ky }, 84.0f, 0.62f, "GAIN");          x += 106.0f;
        ampKnob (g, { x, ky }, 58.0f, 0.3f, "TIGHT");          x += 86.0f;
        ampKnob (g, { x, ky }, 58.0f, 0.55f, "BASS");          x += 86.0f;
        ampKnob (g, { x, ky }, 58.0f, 0.45f, "MID");           x += 86.0f;
        ampKnob (g, { x, ky }, 58.0f, 0.6f, "TREBLE");         x += 86.0f;
        ampKnob (g, { x, ky }, 58.0f, 0.55f, "PRESENCE");      x += 106.0f;
        ampKnob (g, { x, ky }, 84.0f, 0.5f, "MASTER");

        // pedalboard on the floor in front of the cab
        const auto board = juce::Rectangle<float> (40.0f, 496.0f, 1120.0f, 246.0f);
        draw::pedalboard (g, board);

        const float py = 512.0f, ph = 214.0f;
        const auto ink = juce::Colour (0xff1c0e06);

        auto drop = draw::pedalEnclosure (g, { 64.0f, py, 274.0f, ph }, juce::Colour (0xffd2611f));
        {
            const auto disp = juce::Rectangle<float> (drop.getX() + 14.0f, drop.getY() + 14.0f, 128.0f, 72.0f);
            draw::displayGlass (g, disp, 6.0f);
            draw::sevenSegment (g, "-7", disp.reduced (12.0f, 11.0f), colours::amber, 3);
            pedalKnob (g, { drop.getRight() - 50.0f, drop.getY() + 52.0f }, 48.0f, 1.0f, "BODY", ink);
            pedalKnob (g, { drop.getX() + 46.0f, drop.getY() + 132.0f }, 36.0f, 0.3f, "SUB", ink);
            draw::silkscreen (g, "DROP", Fonts::display (30.0f, 0.18f), { drop.getRight() - 120.0f, drop.getY() + 100.0f, 108.0f, 32.0f },
                              juce::Justification::centredRight, ink);
            draw::led (g, { drop.getCentreX() - 5.0f, drop.getBottom() - 74.0f, 10.0f, 10.0f }, true, juce::Colour (0xffff3b1f));
            draw::footswitch (g, juce::Rectangle<float> (42.0f, 42.0f).withCentre ({ drop.getCentreX(), drop.getBottom() - 32.0f }), false);
        }

        auto gate = draw::pedalEnclosure (g, { 364.0f, py, 196.0f, ph }, juce::Colour (0xff353b44));
        {
            pedalKnob (g, { gate.getX() + 48.0f, gate.getY() + 46.0f }, 44.0f, 0.47f, "THRESH", colours::bone);
            pedalKnob (g, { gate.getRight() - 48.0f, gate.getY() + 46.0f }, 44.0f, 0.1f, "HOLD", colours::bone);
            draw::silkscreen (g, "GATE", Fonts::display (26.0f, 0.2f), { gate.getX(), gate.getY() + 84.0f, gate.getWidth(), 30.0f },
                              juce::Justification::centred, colours::bone);
            draw::led (g, { gate.getCentreX() - 5.0f, gate.getBottom() - 74.0f, 10.0f, 10.0f }, true, juce::Colour (0xffff3b1f));
            draw::footswitch (g, juce::Rectangle<float> (42.0f, 42.0f).withCentre ({ gate.getCentreX(), gate.getBottom() - 32.0f }), false);
        }

        auto boost = draw::pedalEnclosure (g, { 586.0f, py, 236.0f, ph }, juce::Colour (0xff1f6040));
        {
            pedalKnob (g, { boost.getX() + 44.0f, boost.getY() + 46.0f }, 42.0f, 0.35f, "DRIVE", colours::bone);
            pedalKnob (g, { boost.getCentreX(), boost.getY() + 46.0f }, 42.0f, 0.6f, "TONE", colours::bone);
            pedalKnob (g, { boost.getRight() - 44.0f, boost.getY() + 46.0f }, 42.0f, 0.62f, "LEVEL", colours::bone);
            draw::silkscreen (g, "BOOST", Fonts::display (26.0f, 0.2f), { boost.getX(), boost.getY() + 84.0f, boost.getWidth(), 30.0f },
                              juce::Justification::centred, colours::bone);
            draw::led (g, { boost.getCentreX() - 5.0f, boost.getBottom() - 74.0f, 10.0f, 10.0f }, false, juce::Colour (0xffff3b1f));
            draw::footswitch (g, juce::Rectangle<float> (42.0f, 42.0f).withCentre ({ boost.getCentreX(), boost.getBottom() - 32.0f }), false);
        }

        auto cab = draw::pedalEnclosure (g, { 848.0f, py, 288.0f, ph }, juce::Colour (0xff1c1c1f));
        {
            const auto lcd = juce::Rectangle<float> (cab.getX() + 14.0f, cab.getY() + 16.0f, cab.getWidth() - 28.0f, 40.0f);
            draw::displayGlass (g, lcd, 5.0f);
            g.setColour (colours::amber);
            g.setFont (Fonts::monoBold (14.0f));
            g.drawText ("01  ASHEN  4x12", lcd.reduced (12.0f, 0.0f), juce::Justification::centredLeft);
            pedalKnob (g, { cab.getX() + 56.0f, cab.getY() + 112.0f }, 42.0f, 1.0f, "MIX", colours::bone);
            pedalKnob (g, { cab.getCentreX(), cab.getY() + 112.0f }, 42.0f, 0.25f, "LOW CUT", colours::bone);
            pedalKnob (g, { cab.getRight() - 56.0f, cab.getY() + 112.0f }, 42.0f, 0.5f, "AIR", colours::bone);
            draw::silkscreen (g, "CAB", Fonts::display (26.0f, 0.2f), { cab.getX() + 16.0f, cab.getBottom() - 46.0f, 100.0f, 30.0f },
                              juce::Justification::centredLeft, colours::bone);
        }
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String out = argc > 1 ? argv[1] : "composition.png";
    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 1.0f;

    Composition comp;
    const auto img = comp.createComponentSnapshot (comp.getLocalBounds(), true, scale);
    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (out));
    file.deleteFile();
    juce::FileOutputStream os (file);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}
