#include "apex/ui/LookAndFeel.h"
#include "apex/ui/Controls.h"
#include "apex/ui/Materials.h"
#include "apex/ui/Theme.h"

#include <cmath>

namespace apex::ui
{

ApexLookAndFeel::ApexLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff121214));
    setColour (juce::PopupMenu::textColourId,                  colours::bone);
    setColour (juce::PopupMenu::headerTextColourId,            colours::dim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff26262b));
    setColour (juce::PopupMenu::highlightedTextColourId,       colours::bone);

    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf20d0d0f));
    setColour (juce::TooltipWindow::textColourId,       colours::bone);
    setColour (juce::TooltipWindow::outlineColourId,    colours::hairline);

    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff121214));
    setColour (juce::AlertWindow::textColourId,       colours::bone);
    setColour (juce::AlertWindow::outlineColourId,    colours::hairline);

    setColour (juce::TextEditor::backgroundColourId,      juce::Colour (0xff09090a));
    setColour (juce::TextEditor::textColourId,            colours::bone);
    setColour (juce::TextEditor::outlineColourId,         colours::hairline);
    setColour (juce::TextEditor::focusedOutlineColourId,  colours::amber.withAlpha (0.7f));
    setColour (juce::TextEditor::highlightColourId,       colours::amber.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId,       colours::amber);

    setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff17171a));
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2b2b30));
    setColour (juce::TextButton::textColourOffId,  colours::label);
    setColour (juce::TextButton::textColourOnId,   colours::bone);

    setColour (juce::ResizableWindow::backgroundColourId, colours::stageBottom);
}

void ApexLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    auto style = KnobStyle::spunAluminium;
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    if (auto* knob = dynamic_cast<Knob*> (&slider))
    {
        style = knob->getStyle();
        area = knob->getKnobArea();
    }

    const float angle = startAngle + pos * (endAngle - startAngle);
    draw::knob (g, area, angle, style);

    if (slider.hasKeyboardFocus (false) && slider.isShowing())
    {
        g.setColour (colours::amber.withAlpha (0.5f));
        const auto d = juce::jmin (area.getWidth(), area.getHeight());
        g.drawEllipse (area.withSizeKeepingCentre (d + 6.0f, d + 6.0f), 1.0f);
    }
}

juce::Font ApexLookAndFeel::getSliderPopupFont (juce::Slider&)
{
    return Fonts::monoBold (13.0f);
}

int ApexLookAndFeel::getSliderPopupPlacement (juce::Slider&)
{
    return juce::BubbleComponent::above;
}

void ApexLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&,
                                  const juce::Rectangle<float>& body)
{
    const auto b = body.reduced (0.5f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 8, { 0, 3 }).drawForRectangle (g, b.toNearestInt());
    g.setColour (juce::Colour (0xf0080809));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (colours::amber.withAlpha (0.55f));
    g.drawRoundedRectangle (b, 5.0f, 1.0f);
}

void ApexLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (colours::hairline);
    g.drawRect (0, 0, width, height, 1);
}

void ApexLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                         bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                         const juce::String& text, const juce::String& shortcutKeyText,
                                         const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (colours::hairline);
        g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    auto r = area.reduced (4, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }

    if (isTicked)
    {
        g.setColour (colours::amber);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) r.getX() + 12.0f, (float) r.getCentreY() }));
    }

    g.setColour (textColour != nullptr ? *textColour : (isActive ? colours::bone : colours::faint));
    g.setFont (getPopupMenuFont());
    auto textArea = r.withTrimmedLeft (24).withTrimmedRight (12);
    g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (colours::dim);
        g.setFont (Fonts::mono (11.0f));
        g.drawText (shortcutKeyText, textArea, juce::Justification::centredRight);
    }

    if (hasSubMenu)
    {
        juce::Path arrow;
        const float cx = (float) r.getRight() - 10.0f, cy = (float) r.getCentreY();
        arrow.startNewSubPath (cx - 3.0f, cy - 4.0f);
        arrow.lineTo (cx + 1.0f, cy);
        arrow.lineTo (cx - 3.0f, cy + 4.0f);
        g.setColour (colours::dim);
        g.strokePath (arrow, juce::PathStrokeType (1.4f));
    }

    juce::ignoreUnused (icon);
}

void ApexLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                  const juce::String& sectionName)
{
    g.setFont (Fonts::label (11.0f, 0.3f));
    g.setColour (colours::dim);
    g.drawFittedText (sectionName.toUpperCase(), area.withTrimmedLeft (14).withTrimmedBottom (2),
                      juce::Justification::bottomLeft, 1);
}

juce::Font ApexLookAndFeel::getPopupMenuFont()
{
    return Fonts::body (16.0f);
}

void ApexLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto b = juce::Rectangle<float> ((float) width, (float) height);
    g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (findColour (juce::TooltipWindow::outlineColourId));
    g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (colours::bone);
    g.setFont (Fonts::body (14.0f));
    g.drawFittedText (text, b.reduced (10.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> ApexLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                        juce::Rectangle<int> parentArea)
{
    const auto font = Fonts::body (14.0f);
    const int w = juce::jmin (320, (int) juce::GlyphArrangement::getStringWidth (font, tipText) + 24);
    const int lines = juce::jmax (1, (int) std::ceil ((juce::GlyphArrangement::getStringWidth (font, tipText) + 24) / 320.0f));
    const int h = 10 + lines * 18;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6,
                                 w, h).constrainedWithin (parentArea);
}

juce::Font ApexLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return Fonts::labelBold (juce::jmin (15.0f, (float) buttonHeight * 0.5f), 0.14f);
}

void ApexLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                                            bool over, bool down)
{
    auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();
    auto fill = on ? findColour (juce::TextButton::buttonOnColourId) : background;
    if (down)      fill = fill.brighter (0.08f);
    else if (over) fill = fill.brighter (0.04f);
    g.setColour (fill);
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (on ? colours::amber.withAlpha (0.55f) : colours::hairline.brighter (over ? 0.3f : 0.0f));
    g.drawRoundedRectangle (b, 5.0f, 1.0f);
}

void ApexLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                            : juce::TextButton::textColourOffId)
                     .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

juce::Font ApexLookAndFeel::getAlertWindowTitleFont()   { return Fonts::labelBold (20.0f, 0.12f); }
juce::Font ApexLookAndFeel::getAlertWindowMessageFont() { return Fonts::body (16.0f); }
juce::Font ApexLookAndFeel::getAlertWindowFont()        { return Fonts::body (15.0f); }

} // namespace apex::ui
