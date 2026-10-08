#pragma once

#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "apex/licence/Licence.h"

#include <atomic>
#include <optional>

namespace apex::ui
{

/**
 * Licensing
 * ---------
 * One per process (hold it in a juce::SharedResourcePointer): the licence key
 * and the trial, shared by every Apex plugin instance.
 *
 * - A licence key (see apex/licence/Licence.h) is checked offline and kept in
 *   the Apex folder, so it unlocks every DAW on the computer.
 * - Without one, the first run starts a 14-day trial with everything working.
 * - After the trial a plugin passes its input through untouched; sessions keep
 *   every setting and sound exactly as before once a key is entered.
 *
 * Message thread only, except isAllowed(), which the audio thread may call.
 */
class Licensing : public juce::ChangeBroadcaster
{
public:
    static constexpr int trialDays = 14;

    struct Status
    {
        bool licensed = false;      // a valid key for this product
        bool expired = false;       // no key and the trial is over
        int daysLeft = 0;           // of the trial
        juce::String owner;
        std::uint32_t serial = 0;
        bool development = false;   // built with the development key: not for sale
    };

    /** Keeps its files in `folder` (the default is the shared Apex folder). */
    explicit Licensing (juce::File folder = defaultFolder());

    Status getStatus (std::uint8_t product) const;
    bool isAllowed (std::uint8_t product) const noexcept;

    /** Checks and stores a key. Returns an error to show, or an empty string. */
    juce::String activate (const juce::String& keyText, std::uint8_t product);
    void removeLicence();

    /** Re-reads the key (another plugin may have activated) and re-checks the
        trial clock. Cheap; call it from a timer. */
    void refresh();

    /** Where the key and the trial live: Application Support/Apex (mac),
        AppData/Roaming/Apex (Windows), ~/.config/Apex (Linux). */
    static juce::File defaultFolder();
    juce::File getFolder() const { return dir; }

    /** Writes a trial that started at `startMs` (tests). */
    static void writeTrial (const juce::File& folder, juce::int64 startMs);

private:
    void loadLicence();
    void loadOrStartTrial();
    void updateAllowed();
    int trialDaysLeft() const;

    juce::File dir;
    std::optional<apex::licence::Licence> licence;
    juce::int64 trialStartMs = 0;
    bool trialValid = true;
    juce::Time licenceFileTime;
    std::atomic<std::uint8_t> allowedProducts { 0 };
};

/**
 * The unlock screen, in the abyss style: what the trial or licence is doing,
 * a field to paste a key, Activate, Load Key File and Buy. Covers its parent;
 * the host editor shows it from a "trial" badge or the settings menu.
 */
class UnlockOverlay : public juce::Component,
                      private juce::ChangeListener
{
public:
    UnlockOverlay (Licensing&, std::uint8_t product, juce::String productName, juce::String storeUrl);
    ~UnlockOverlay() override;

    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void update();
    void tryActivate (const juce::String& text);
    juce::Rectangle<float> plate() const;

    Licensing& licensing;
    std::uint8_t product;
    juce::String productName, storeUrl, message;
    bool messageIsError = false;

    juce::TextEditor keyField;
    std::unique_ptr<juce::Button> activateButton, fileButton, buyButton, closeButton, removeButton;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace apex::ui
