#include "apex/ui/Objects.h"
#include "apex/ui/Materials.h"
#include "apex/ui/Theme.h"

namespace apex::ui::draw
{

void stage (juce::Graphics& g, juce::Rectangle<float> b, juce::Point<float> focus)
{
    const juce::Graphics::ScopedSaveState saved (g);

    g.setGradientFill (juce::ColourGradient (colours::stageTop, b.getX(), b.getY(),
                                             colours::stageBottom, b.getX(), b.getBottom(), false));
    g.fillRect (b);

    // key light behind the amp
    juce::ColourGradient key (juce::Colour (0xff3a3a40).withAlpha (0.55f), focus.x, focus.y,
                              juce::Colours::transparentBlack, focus.x + b.getWidth() * 0.55f, focus.y, true);
    g.setGradientFill (key);
    g.fillRect (b);

    // vignette
    juce::ColourGradient vignette (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                                   juce::Colours::black.withAlpha (0.55f), b.getX() - b.getWidth() * 0.1f, b.getY(), true);
    g.setGradientFill (vignette);
    g.fillRect (b);
}

void pedalboard (juce::Graphics& g, juce::Rectangle<float> b)
{
    const juce::Graphics::ScopedSaveState saved (g);
    contactShadow (g, b, 22.0f, 0.75f);

    juce::Path deck;
    deck.addRoundedRectangle (b, 10.0f);
    texturedFill (g, deck, juce::Colour (0xff111113), materials::tolexGrain(), 0.35f, 0.5f);

    // aluminium rails top and bottom
    for (auto rail : { b.removeFromTop (10.0f), b.withTrimmedTop (b.getHeight() - 10.0f) })
    {
        juce::ColourGradient al (juce::Colour (0xff5d5e63), 0.0f, rail.getY(), juce::Colour (0xff1e1e21), 0.0f, rail.getBottom(), false);
        al.addColour (0.35, juce::Colour (0xff9fa0a5));
        g.setGradientFill (al);
        g.fillRoundedRectangle (rail, 4.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);
}

namespace
{
    float pedalBevel (juce::Rectangle<float> b) { return juce::jmax (4.0f, juce::jmin (b.getWidth(), b.getHeight()) * 0.045f); }
    constexpr float handleHeight = 24.0f;
}

juce::Rectangle<float> pedalTopFace (juce::Rectangle<float> b)
{
    return b.reduced (pedalBevel (b));
}

juce::Rectangle<float> ampHeadFaceplate (juce::Rectangle<float> b)
{
    return b.withTrimmedTop (handleHeight * 0.7f).reduced (40.0f, 34.0f);
}

juce::Point<float> topJackPlug (juce::Graphics& g, juce::Point<float> tip, float w)
{
    const juce::Graphics::ScopedSaveState saved (g);
    const float barrelLen = w * 0.9f, bootLen = w * 1.6f;
    const auto barrel = juce::Rectangle<float> (w * 0.78f, barrelLen).withCentre ({ tip.x, tip.y - barrelLen * 0.5f });
    const auto boot   = juce::Rectangle<float> (w, bootLen).withCentre ({ tip.x, tip.y - barrelLen - bootLen * 0.5f });

    juce::DropShadow (juce::Colours::black.withAlpha (0.65f), juce::roundToInt (w * 0.6f), { juce::roundToInt (w * 0.15f), juce::roundToInt (w * 0.35f) })
        .drawForRectangle (g, boot.getUnion (barrel).toNearestInt());

    juce::ColourGradient chrome (juce::Colour (0xff3d3e42), barrel.getX(), 0.0f, juce::Colour (0xff2a2b2e), barrel.getRight(), 0.0f, false);
    chrome.addColour (0.3, juce::Colour (0xffeeeeef));
    chrome.addColour (0.55, juce::Colour (0xff9fa0a5));
    g.setGradientFill (chrome);
    g.fillRoundedRectangle (barrel, w * 0.1f);

    juce::ColourGradient rubber (juce::Colour (0xff2e2e33), boot.getX(), 0.0f, juce::Colour (0xff070708), boot.getRight(), 0.0f, false);
    rubber.addColour (0.3, juce::Colour (0xff45454b));
    g.setGradientFill (rubber);
    g.fillRoundedRectangle (boot, w * 0.28f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    for (int i = 1; i < 5; ++i)
    {
        const float y = boot.getY() + boot.getHeight() * (float) i / 5.0f;
        g.drawLine (boot.getX() + w * 0.12f, y, boot.getRight() - w * 0.12f, y, 0.8f);
    }
    return { tip.x, boot.getY() + w * 0.2f };
}

juce::Rectangle<float> pedalEnclosure (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour paint)
{
    const juce::Graphics::ScopedSaveState saved (g);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.07f;
    const float bevel = pedalBevel (b);

    contactShadow (g, b, 16.0f, 0.85f);

    // body + bevel: darker sides, lit top-left edge
    juce::Path body;
    body.addRoundedRectangle (b, r);
    g.setGradientFill (juce::ColourGradient (paint.darker (0.35f), b.getX(), b.getY(),
                                             paint.darker (1.4f), b.getRight(), b.getBottom(), false));
    g.fillPath (body);

    juce::ColourGradient edge (juce::Colours::white.withAlpha (0.28f), b.getX(), b.getY(),
                               juce::Colours::transparentWhite, b.getX() + bevel * 3.0f, b.getY() + bevel * 3.0f, false);
    g.setGradientFill (edge);
    g.strokePath (body, juce::PathStrokeType (1.4f));

    // top face
    const auto top = b.reduced (bevel);
    juce::Path face;
    face.addRoundedRectangle (top, r * 0.7f);
    juce::ColourGradient paintGrad (paint.brighter (0.22f), top.getX(), top.getY(),
                                    paint.darker (0.45f), top.getRight(), top.getBottom(), false);
    paintGrad.addColour (0.45, paint);
    g.setGradientFill (paintGrad);
    g.fillPath (face);

    // metallic flake
    {
        const juce::Graphics::ScopedSaveState s2 (g);
        juce::FillType ft (materials::powderGrain(), juce::AffineTransform::scale (0.4f));
        ft.setOpacity (0.38f);
        g.setFillType (ft);
        g.fillPath (face);
    }
    // clear-coat: broad soft sheen plus a tighter glossy band
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.20f), top.getX(), top.getY(),
                                juce::Colours::transparentWhite, top.getX() + top.getWidth() * 0.6f, top.getY() + top.getHeight() * 0.5f, false);
    g.setGradientFill (sheen);
    g.fillPath (face);
    {
        const juce::Graphics::ScopedSaveState s2 (g);
        g.reduceClipRegion (face);
        juce::Path band;
        band.startNewSubPath (top.getX() - 20.0f, top.getY() + top.getHeight() * 0.18f);
        band.lineTo (top.getX() + top.getWidth() * 0.55f, top.getY() - 20.0f);
        band.lineTo (top.getX() + top.getWidth() * 0.72f, top.getY() - 20.0f);
        band.lineTo (top.getX() - 20.0f, top.getY() + top.getHeight() * 0.42f);
        band.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillPath (band);
    }

    // crisp edge between bevel and face
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (face, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (0.5f, 0.8f));
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.strokePath (face, juce::PathStrokeType (0.8f), juce::AffineTransform::translation (-0.3f, -0.4f));
    return top;
}

juce::Rectangle<float> ampHead (juce::Graphics& g, juce::Rectangle<float> b)
{
    const juce::Graphics::ScopedSaveState saved (g);

    // leather handle on top
    const float handleW = b.getWidth() * 0.24f, handleH = handleHeight;
    const auto handle = juce::Rectangle<float> (handleW, handleH).withCentre ({ b.getCentreX(), b.getY() + handleH * 0.5f });
    const auto box = b.withTrimmedTop (handleH * 0.7f);

    contactShadow (g, box, 26.0f, 0.9f);

    {
        juce::Path strap;
        strap.addRoundedRectangle (handle, handleH * 0.45f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 8, { 0, 3 }).drawForPath (g, strap);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2622), 0.0f, handle.getY(),
                                                 juce::Colour (0xff0b0a09), 0.0f, handle.getBottom(), false));
        g.fillPath (strap);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        const float dash[] = { 3.0f, 3.0f };
        juce::Path stitch;
        juce::PathStrokeType (0.8f).createDashedStroke (stitch, [&] {
            juce::Path p; p.addRoundedRectangle (handle.reduced (3.5f), handleH * 0.3f); return p; }(), dash, 2);
        g.fillPath (stitch);
        for (float sx : { handle.getX() - 6.0f, handle.getRight() - 12.0f })
        {
            const auto cap = juce::Rectangle<float> (sx, handle.getY() + handleH * 0.15f, 18.0f, handleH * 0.95f);
            juce::ColourGradient chrome (juce::Colour (0xffd5d6da), cap.getX(), cap.getY(), juce::Colour (0xff3a3b3f), cap.getX(), cap.getBottom(), false);
            chrome.addColour (0.5, juce::Colour (0xff8a8b90));
            g.setGradientFill (chrome);
            g.fillRoundedRectangle (cap, 3.0f);
        }
    }

    // tolex box
    const float r = 16.0f;
    juce::Path shell;
    shell.addRoundedRectangle (box, r);
    texturedFill (g, shell, colours::tolex, materials::tolexGrain(), 0.55f, 0.6f);
    juce::ColourGradient form (juce::Colours::white.withAlpha (0.07f), box.getX(), box.getY(),
                               juce::Colours::black.withAlpha (0.35f), box.getX(), box.getBottom(), false);
    g.setGradientFill (form);
    g.fillPath (shell);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.strokePath (shell, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (0.0f, 0.6f));

    // corner protectors
    const float cp = 42.0f;
    auto corner = [&] (float x, float y, float sx, float sy)
    {
        juce::Path p;
        p.startNewSubPath (0.0f, 0.0f);
        p.lineTo (cp, 0.0f);
        p.quadraticTo (cp * 0.95f, cp * 0.32f, cp * 0.55f, cp * 0.4f);
        p.quadraticTo (cp * 0.4f, cp * 0.55f, cp * 0.32f, cp * 0.95f);
        p.lineTo (0.0f, cp);
        p.closeSubPath();
        p.applyTransform (juce::AffineTransform::scale (sx, sy).translated (x, y));
        const auto pb = p.getBounds();
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 6, { 0, 2 }).drawForPath (g, p);
        juce::ColourGradient chrome (juce::Colour (0xff2f3034), pb.getX(), pb.getY(), juce::Colour (0xff0d0d0f), pb.getRight(), pb.getBottom(), false);
        chrome.addColour (0.3, juce::Colour (0xff5e5f64));
        g.setGradientFill (chrome);
        const juce::Graphics::ScopedSaveState s3 (g);
        g.reduceClipRegion (shell);
        g.fillPath (p);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.strokePath (p, juce::PathStrokeType (0.8f));
        screw (g, { pb.getX() + (sx > 0 ? cp * 0.24f : pb.getWidth() - cp * 0.24f), pb.getY() + (sy > 0 ? cp * 0.24f : pb.getHeight() - cp * 0.24f) },
               2.6f, 0.6f);
    };
    corner (box.getX(), box.getY(), 1.0f, 1.0f);
    corner (box.getRight(), box.getY(), -1.0f, 1.0f);
    corner (box.getX(), box.getBottom(), 1.0f, -1.0f);
    corner (box.getRight(), box.getBottom(), -1.0f, -1.0f);

    // faceplate cut-out with cream piping
    const auto plate = ampHeadFaceplate (b);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillRoundedRectangle (plate.expanded (6.0f), 8.0f);
    juce::Path piping;
    piping.addRoundedRectangle (plate.expanded (4.0f), 7.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (piping, juce::PathStrokeType (2.6f), juce::AffineTransform::translation (0.0f, 0.8f));
    g.setGradientFill (juce::ColourGradient (colours::piping, plate.getX(), plate.getY(),
                                             colours::piping.darker (0.5f), plate.getX(), plate.getBottom(), false));
    g.strokePath (piping, juce::PathStrokeType (1.8f));
    return plate;
}

juce::Rectangle<float> speakerCab (juce::Graphics& g, juce::Rectangle<float> b)
{
    const juce::Graphics::ScopedSaveState saved (g);
    const float r = 16.0f;
    juce::Path shell;
    shell.addRoundedRectangle (b, r);
    texturedFill (g, shell, colours::tolex, materials::tolexGrain(), 0.55f, 0.6f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), b.getX(), b.getY(),
                                             juce::Colours::black.withAlpha (0.5f), b.getX(), b.getY() + b.getHeight() * 0.6f, false));
    g.fillPath (shell);

    const auto grille = b.reduced (38.0f, 34.0f);
    juce::Path cloth;
    cloth.addRoundedRectangle (grille, 6.0f);
    texturedFill (g, cloth, juce::Colour (0xff0e0e10), materials::grilleCloth(), 0.6f, 0.5f);
    // four 12" speakers faintly visible through the cloth
    {
        const juce::Graphics::ScopedSaveState s2 (g);
        g.reduceClipRegion (cloth);
        const float rs = grille.getWidth() * 0.19f;
        for (int i = 0; i < 4; ++i)
        {
            const juce::Point<float> c (grille.getX() + grille.getWidth() * (i % 2 == 0 ? 0.27f : 0.73f),
                                        grille.getY() + grille.getHeight() * (i < 2 ? 0.3f : 0.76f));
            juce::ColourGradient cone (juce::Colours::black.withAlpha (0.05f), c.x, c.y,
                                       juce::Colours::black.withAlpha (0.42f), c.x + rs, c.y, true);
            cone.addColour (0.18, juce::Colours::white.withAlpha (0.035f));   // dust cap
            cone.addColour (0.24, juce::Colours::black.withAlpha (0.25f));
            cone.addColour (0.92, juce::Colours::black.withAlpha (0.30f));
            g.setGradientFill (cone);
            g.fillEllipse (juce::Rectangle<float> (rs * 2.0f, rs * 2.0f).withCentre (c));
            g.setColour (juce::Colours::white.withAlpha (0.035f));
            g.drawEllipse (juce::Rectangle<float> (rs * 2.0f, rs * 2.0f).withCentre (c), 2.0f);
        }
    }

    // baffle depth: shade from the top edge (the head casts a shadow) and the sides
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.75f), grille.getX(), grille.getY(),
                                             juce::Colours::transparentBlack, grille.getX(), grille.getY() + 60.0f, false));
    g.fillPath (cloth);
    juce::ColourGradient spot (juce::Colours::white.withAlpha (0.05f), grille.getCentreX(), grille.getY() + 40.0f,
                               juce::Colours::transparentWhite, grille.getRight(), grille.getY() + 40.0f, true);
    g.setGradientFill (spot);
    g.fillPath (cloth);

    juce::Path piping;
    piping.addRoundedRectangle (grille.expanded (4.0f), 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (piping, juce::PathStrokeType (2.6f), juce::AffineTransform::translation (0.0f, 0.8f));
    g.setGradientFill (juce::ColourGradient (colours::piping, grille.getX(), grille.getY(),
                                             colours::piping.darker (0.6f), grille.getX(), grille.getBottom(), false));
    g.strokePath (piping, juce::PathStrokeType (1.8f));
    return grille;
}

void faceplate (juce::Graphics& g, juce::Rectangle<float> a)
{
    const juce::Graphics::ScopedSaveState saved (g);
    juce::Path plate;
    plate.addRoundedRectangle (a, 5.0f);
    texturedFill (g, plate, colours::faceplate, materials::brushedMetal(), 0.55f, 1.0f);

    // broad horizontal light falloff + chamfer
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), a.getX(), a.getY(),
                                             juce::Colours::black.withAlpha (0.25f), a.getX(), a.getBottom(), false));
    g.fillPath (plate);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawLine (a.getX() + 5.0f, a.getY() + 0.8f, a.getRight() - 5.0f, a.getY() + 0.8f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawLine (a.getX() + 5.0f, a.getBottom() - 0.6f, a.getRight() - 5.0f, a.getBottom() - 0.6f, 1.2f);

    const float m = 12.0f;
    screw (g, { a.getX() + m, a.getY() + m }, 4.2f, 0.5f);
    screw (g, { a.getRight() - m, a.getY() + m }, 4.2f, 1.9f);
    screw (g, { a.getX() + m, a.getBottom() - m }, 4.2f, 2.6f);
    screw (g, { a.getRight() - m, a.getBottom() - m }, 4.2f, 0.9f);
}

void engravedRule (juce::Graphics& g, juce::Rectangle<float> a, const juce::String& caption, juce::Colour colour)
{
    const juce::Graphics::ScopedSaveState saved (g);
    const float y = a.getCentreY();
    float gapL = a.getCentreX(), gapR = a.getCentreX();
    if (caption.isNotEmpty())
    {
        const auto font = Fonts::label (a.getHeight() * 0.9f, 0.3f);
        const float tw = juce::GlyphArrangement::getStringWidth (font, caption) + 14.0f;
        gapL -= tw * 0.5f;
        gapR += tw * 0.5f;
        silkscreen (g, caption, font, a, juce::Justification::centred, colour);
    }
    for (auto seg : { juce::Line<float> (a.getX(), y, gapL, y), juce::Line<float> (gapR, y, a.getRight(), y) })
    {
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (seg.getStartX(), seg.getStartY() + 0.8f, seg.getEndX(), seg.getEndY() + 0.8f, 1.0f);
        g.setColour (colour.withAlpha (0.55f));
        g.drawLine (seg, 1.0f);
    }
}

} // namespace apex::ui::draw
