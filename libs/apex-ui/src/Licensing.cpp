#include "apex/ui/Licensing.h"
#include "apex/ui/Abyss.h"

namespace apex::ui
{

namespace
{
    constexpr juce::int64 dayMs = 24 * 60 * 60 * 1000;

    // The trial file carries a checksum so an edited date reads as an ended
    // trial. It only keeps honest people honest, which is all a trial needs.
    juce::String trialCheck (juce::int64 startMs)
    {
        const juce::String text = "apex/trial/" + juce::String (startMs) + "/thallbyssal";
        std::uint64_t h = 1469598103934665603ull;
        for (auto c : text)
        {
            h ^= (std::uint64_t) c;
            h *= 1099511628211ull;
        }
        return juce::String::toHexString ((juce::int64) h);
    }

    juce::File licenceFileIn (const juce::File& dir) { return dir.getChildFile ("licence.key"); }
    juce::File trialFileIn (const juce::File& dir)   { return dir.getChildFile ("trial.dat"); }
}

void Licensing::writeTrial (const juce::File& folder, juce::int64 startMs)
{
    folder.createDirectory();
    trialFileIn (folder).replaceWithText ("APEX-TRIAL-1 " + juce::String (startMs) + " " + trialCheck (startMs));
}

juce::File Licensing::defaultFolder()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support/Apex");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Apex");
   #endif
}

Licensing::Licensing (juce::File folder) : dir (std::move (folder))
{
    loadLicence();
    loadOrStartTrial();
    updateAllowed();
}

void Licensing::loadLicence()
{
    const auto file = licenceFileIn (dir);
    licenceFileTime = file.getLastModificationTime();
    licence.reset();
    if (file.existsAsFile())
        licence = apex::licence::verify (file.loadFileAsString().toStdString(), apex::licence::builtInPublicKey());
}

void Licensing::loadOrStartTrial()
{
    const auto file = trialFileIn (dir);
    const auto now = juce::Time::currentTimeMillis();
    if (file.existsAsFile())
    {
        const auto parts = juce::StringArray::fromTokens (file.loadFileAsString().trim(), " ", {});
        if (parts.size() == 3 && parts[0] == "APEX-TRIAL-1")
        {
            trialStartMs = parts[1].getLargeIntValue();
            trialValid = parts[2] == trialCheck (trialStartMs);
            return;
        }
        trialStartMs = now;
        trialValid = false;
        return;
    }
    // first run on this computer
    trialStartMs = now;
    trialValid = true;
    writeTrial (dir, now);
}

int Licensing::trialDaysLeft() const
{
    const auto elapsed = juce::Time::currentTimeMillis() - trialStartMs;
    // a clock set back more than two days, or an edited trial file, ends the trial
    if (! trialValid || elapsed < -2 * dayMs)
        return 0;
    return juce::jlimit (0, trialDays, trialDays - (int) (juce::jmax ((juce::int64) 0, elapsed) / dayMs));
}

Licensing::Status Licensing::getStatus (std::uint8_t product) const
{
    Status s;
    s.development = apex::licence::isDevelopmentBuild();
    if (licence && licence->unlocks (product))
    {
        s.licensed = true;
        s.owner = juce::String::fromUTF8 (licence->owner.c_str());
        s.serial = licence->serial;
        return s;
    }
    s.daysLeft = trialDaysLeft();
    s.expired = s.daysLeft <= 0;
    return s;
}

bool Licensing::isAllowed (std::uint8_t product) const noexcept
{
    return (allowedProducts.load (std::memory_order_relaxed) & product) != 0;
}

void Licensing::updateAllowed()
{
    std::uint8_t allowed = licence ? licence->products : 0;
    if (trialDaysLeft() > 0)
        allowed = apex::licence::product::all;
    if (allowed != allowedProducts.exchange (allowed))
        sendChangeMessage();
}

juce::String Licensing::activate (const juce::String& keyText, std::uint8_t product)
{
    const auto parsed = apex::licence::verify (keyText.toStdString(), apex::licence::builtInPublicKey());
    if (! parsed)
        return "That is not a valid key. Paste all of it, from APEX to the end.";
    if (! parsed->unlocks (product))
        return "This key is for another Apex plugin.";

    licence = parsed;
    dir.createDirectory();
    const bool saved = licenceFileIn (dir).replaceWithText (keyText.trim());
    licenceFileTime = licenceFileIn (dir).getLastModificationTime();
    updateAllowed();
    sendChangeMessage();
    return saved ? juce::String() : "Unlocked, but the key could not be saved in " + dir.getFullPathName()
                                        + ". It works until the DAW is closed.";
}

void Licensing::removeLicence()
{
    licenceFileIn (dir).deleteFile();
    licence.reset();
    licenceFileTime = {};
    updateAllowed();
    sendChangeMessage();
}

void Licensing::refresh()
{
    const auto file = licenceFileIn (dir);
    if (file.getLastModificationTime() != licenceFileTime)
    {
        loadLicence();
        sendChangeMessage();
    }
    updateAllowed();
}

//==============================================================================
namespace col = abyss::colours;
namespace fnt = abyss::fonts;

UnlockOverlay::UnlockOverlay (Licensing& l, std::uint8_t p, juce::String name, juce::String url)
    : licensing (l), product (p), productName (std::move (name)), storeUrl (std::move (url))
{
    keyField.setMultiLine (true, true);
    keyField.setReturnKeyStartsNewLine (false);
    keyField.setFont (fnt::value (17.0f));
    keyField.setJustification (juce::Justification::centred);
    keyField.setTextToShowWhenEmpty ("Paste your licence key here:  APEX-XXXXX-XXXXX-...", col::ash.withAlpha (0.7f));
    keyField.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff060403));
    keyField.setColour (juce::TextEditor::textColourId, col::emberHot);
    keyField.setColour (juce::TextEditor::outlineColourId, col::rim);
    keyField.setColour (juce::TextEditor::focusedOutlineColourId, col::ember.withAlpha (0.8f));
    keyField.onReturnKey = [this] { tryActivate (keyField.getText()); };
    keyField.onTextChange = [this]
    {
        // a complete key activates as soon as it is pasted
        const auto text = keyField.getText();
        if (apex::licence::verify (text.toStdString(), apex::licence::builtInPublicKey()))
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<UnlockOverlay> (this), text]
                                             {
                                                 if (safe != nullptr)
                                                     safe->tryActivate (text);
                                             });
    };
    addAndMakeVisible (keyField);

    auto makeButton = [this] (const juce::String& text, abyss::GlowButton::Style style)
    {
        auto b = std::make_unique<abyss::GlowButton> (text, style);
        addAndMakeVisible (*b);
        return b;
    };
    activateButton = makeButton ("ACTIVATE", abyss::GlowButton::Style::plain);
    activateButton->onClick = [this] { tryActivate (keyField.getText()); };
    fileButton = makeButton ("LOAD KEY FILE", abyss::GlowButton::Style::tab);
    fileButton->onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose your Apex licence key", juce::File(), "*.key;*.txt");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      tryActivate (fc.getResult().loadFileAsString());
                              });
    };
    buyButton = makeButton ("BUY " + productName.toUpperCase(), abyss::GlowButton::Style::tab);
    buyButton->onClick = [this] { juce::URL (storeUrl).launchInDefaultBrowser(); };
    closeButton = makeButton ("CLOSE", abyss::GlowButton::Style::tab);
    closeButton->onClick = [this] { if (onClose) onClose(); };
    removeButton = makeButton ("REMOVE LICENCE", abyss::GlowButton::Style::tab);
    removeButton->onClick = [this]
    {
        licensing.removeLicence();
        message = "The licence was removed from this computer.";
        messageIsError = false;
        update();
    };

    licensing.addChangeListener (this);
    update();
}

UnlockOverlay::~UnlockOverlay()
{
    licensing.removeChangeListener (this);
}

void UnlockOverlay::changeListenerCallback (juce::ChangeBroadcaster*)
{
    update();
}

void UnlockOverlay::tryActivate (const juce::String& text)
{
    const auto error = licensing.activate (text, product);
    const auto status = licensing.getStatus (product);
    messageIsError = ! status.licensed;
    message = error.isNotEmpty() ? error : "Unlocked. Thank you for supporting independent tools.";
    if (status.licensed)
        keyField.clear();
    update();
}

void UnlockOverlay::update()
{
    const auto s = licensing.getStatus (product);
    keyField.setVisible (! s.licensed);
    activateButton->setVisible (! s.licensed);
    fileButton->setVisible (! s.licensed);
    buyButton->setVisible (! s.licensed && storeUrl.isNotEmpty());
    removeButton->setVisible (s.licensed);
    resized();
    repaint();
}

juce::Rectangle<float> UnlockOverlay::plate() const
{
    return getLocalBounds().toFloat().withSizeKeepingCentre (660.0f, 450.0f);
}

void UnlockOverlay::resized()
{
    const auto p = plate();
    keyField.setBounds (juce::Rectangle<float> (p.getX() + 50.0f, p.getY() + 214.0f, p.getWidth() - 100.0f, 84.0f).toNearestInt());

    const float y = p.getY() + 320.0f, h = 40.0f;
    if (activateButton->isVisible())
    {
        const bool buy = buyButton->isVisible();
        const float w = buy ? 172.0f : 230.0f, gap = 14.0f;
        const float total = (buy ? 3.0f : 2.0f) * w + (buy ? 2.0f : 1.0f) * gap;
        float x = p.getCentreX() - total * 0.5f;
        activateButton->setBounds (juce::Rectangle<float> (x, y, w, h).toNearestInt());
        x += w + gap;
        fileButton->setBounds (juce::Rectangle<float> (x, y, w, h).toNearestInt());
        x += w + gap;
        if (buy)
            buyButton->setBounds (juce::Rectangle<float> (x, y, w, h).toNearestInt());
    }
    removeButton->setBounds (juce::Rectangle<float> (p.getCentreX() - 125.0f, p.getY() + 236.0f, 250.0f, h).toNearestInt());
    closeButton->setBounds (juce::Rectangle<float> (p.getCentreX() - 70.0f, p.getBottom() - 54.0f, 140.0f, 34.0f).toNearestInt());
}

void UnlockOverlay::mouseUp (const juce::MouseEvent& e)
{
    if (! plate().contains (e.position) && onClose)
        onClose();
}

void UnlockOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.78f));
    const auto p = plate();
    const auto s = licensing.getStatus (product);

    // the plate: basalt with an ember rim
    const auto outline = abyss::archPanel (p, 26.0f, 10.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff16110e), p.getCentreX(), p.getY(),
                                             juce::Colour (0xff070504), p.getCentreX(), p.getBottom(), false));
    g.fillPath (outline);
    abyss::glowSpot (g, { p.getCentreX(), p.getY() + 60.0f }, 260.0f, col::emberDeep, s.expired ? 0.28f : 0.16f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.strokePath (outline, juce::PathStrokeType (3.0f));
    abyss::glowStroke (g, outline, 1.4f, col::ember, s.expired ? 0.9f : 0.55f);

    abyss::drawIcon (g, abyss::Icon::trident, { p.getCentreX() - 18.0f, p.getY() + 30.0f, 36.0f, 36.0f }, col::ember, 0.6f);
    abyss::glowText (g, (s.licensed ? productName : "UNLOCK " + productName).toUpperCase(), fnt::serif (26.0f, 0.32f),
                     { p.getX(), p.getY() + 74.0f, p.getWidth(), 34.0f }, juce::Justification::centred, col::bone, 0.25f);

    juce::String headline, detail;
    if (s.licensed)
    {
        headline = s.owner.isNotEmpty() ? "LICENSED TO " + s.owner.toUpperCase() : juce::String ("LICENSED");
        detail = "Serial " + juce::String (s.serial) + ". Every DAW on this computer is unlocked.";
    }
    else if (s.expired)
    {
        headline = "THE TRIAL HAS ENDED";
        detail = "Your guitar passes through untouched. Every session keeps its settings\nand sounds exactly as before once you enter a key.";
    }
    else
    {
        headline = "TRIAL  " + juce::String::charToString (0x00B7) + "  " + juce::String (s.daysLeft) + (s.daysLeft == 1 ? " DAY LEFT" : " DAYS LEFT");
        detail = "Everything works during the trial. Enter your licence key to keep it.";
    }
    abyss::glowText (g, headline, fnt::labelBold (21.0f, 0.14f), { p.getX(), p.getY() + 122.0f, p.getWidth(), 26.0f },
                     juce::Justification::centred, s.expired ? col::ember : col::emberHot, 0.5f);
    g.setColour (col::ash);
    g.setFont (fnt::label (15.0f, 0.04f));
    g.drawFittedText (detail, juce::Rectangle<float> (p.getX() + 40.0f, p.getY() + 154.0f, p.getWidth() - 80.0f, 44.0f).toNearestInt(),
                      juce::Justification::centredTop, 2);

    if (message.isNotEmpty())
    {
        const float y = s.licensed ? p.getY() + 290.0f : p.getY() + 368.0f;
        g.setColour (messageIsError ? juce::Colour (0xffff5a3c) : col::emberHot);
        g.setFont (fnt::label (14.0f, 0.04f));
        g.drawFittedText (message, juce::Rectangle<float> (p.getX() + 40.0f, y, p.getWidth() - 80.0f, 26.0f).toNearestInt(),
                          juce::Justification::centred, 2);
    }

    if (s.development)
        abyss::glowText (g, "DEVELOPMENT BUILD  " + juce::String::charToString (0x00B7) + "  NOT FOR SALE", fnt::labelBold (11.0f, 0.2f),
                         { p.getX(), p.getBottom() - 16.0f, p.getWidth(), 12.0f }, juce::Justification::centred, juce::Colour (0xffff5a3c), 0.0f);
}

} // namespace apex::ui
