// The trial and licence states the plugin can be in, against a scratch folder:
// a first run starts a 14-day trial, an old or edited trial has ended, keys
// unlock (and only for their product), a key activated by another plugin
// instance is picked up, and removing it brings the trial state back.
#include "apex/ui/Licensing.h"

#include <cstdio>

using apex::ui::Licensing;
namespace product = apex::licence::product;

namespace
{
    int failures = 0;
    void check (bool ok, const char* what)
    {
        if (! ok)
        {
            std::printf ("  FAIL: %s\n", what);
            ++failures;
        }
    }

    // the development secret key (public on purpose, see libs/apex-licence/dev)
    std::string devKey (const std::string& owner, std::uint8_t products)
    {
        apex::licence::SecretKey sk {};
        apex::licence::fromHex ("97b3847396aead6bd3d00fb4a5c3800afbb474ce1785c9fa983054d6c14785ec"
                                "5755a626740752f71f4ede36c6a46324bca52935f5c5a0de1b9cab94d51ac8e8",
                                sk.data(), sk.size());
        apex::licence::Licence l;
        l.owner = owner;
        l.products = products;
        l.serial = 77;
        return apex::licence::sign (l, sk);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("apex_licensing_check");
    dir.deleteRecursively();
    const auto day = (juce::int64) 24 * 60 * 60 * 1000;
    const auto now = juce::Time::currentTimeMillis();

    {
        Licensing l (dir);
        const auto s = l.getStatus (product::amp);
        std::printf ("first run: %d days left\n", s.daysLeft);
        check (! s.licensed && ! s.expired && s.daysLeft == Licensing::trialDays, "a first run starts a full trial");
        check (l.isAllowed (product::amp) && l.isAllowed (product::drop), "everything works in the trial");
        check (dir.getChildFile ("trial.dat").existsAsFile(), "the trial is remembered");
    }
    {
        Licensing again (dir);
        check (again.getStatus (product::amp).daysLeft == Licensing::trialDays, "a second start keeps the same trial");
    }

    Licensing::writeTrial (dir, now - 10 * day - 1000);
    check (Licensing (dir).getStatus (product::amp).daysLeft == 4, "ten days in, four are left");

    Licensing::writeTrial (dir, now - 15 * day);
    {
        Licensing l (dir);
        const auto s = l.getStatus (product::amp);
        check (s.expired && s.daysLeft == 0 && ! l.isAllowed (product::amp), "after 14 days the trial has ended");

        // a wrong key, a key for another product, then the right one
        check (l.activate ("APEX-12345-ABCDE", product::amp).isNotEmpty() && ! l.isAllowed (product::amp), "garbage is refused");
        check (l.activate (devKey ("Drop Owner", product::drop), product::amp).isNotEmpty() && ! l.isAllowed (product::amp),
               "a key for another product is refused");
        const auto key = devKey ("Test Buyer", product::amp);
        check (l.activate (key, product::amp).isEmpty(), "a valid key activates");
        const auto unlocked = l.getStatus (product::amp);
        check (unlocked.licensed && unlocked.owner == "Test Buyer" && unlocked.serial == 77 && l.isAllowed (product::amp), "licensed");
        check (! l.isAllowed (product::drop), "an amp key does not unlock other products after the trial");
        check (dir.getChildFile ("licence.key").existsAsFile(), "the key is stored");
    }

    // a fresh instance (another DAW) reads the stored key
    {
        Licensing other (dir);
        check (other.getStatus (product::amp).licensed && other.isAllowed (product::amp), "the key unlocks every instance");
        other.removeLicence();
        check (! other.isAllowed (product::amp) && other.getStatus (product::amp).expired, "removing the key returns to the ended trial");
    }

    // an instance that was already running picks up a key entered elsewhere
    {
        Licensing running (dir);
        check (! running.isAllowed (product::amp), "locked before");
        juce::Thread::sleep (1100);   // file times have one-second resolution on some systems
        Licensing (dir).activate (devKey ("Elsewhere", product::all), product::amp);
        running.refresh();
        check (running.isAllowed (product::amp) && running.getStatus (product::amp).owner == "Elsewhere", "refresh picks up a new key");
        running.removeLicence();
    }

    // an edited trial date ends the trial
    {
        Licensing::writeTrial (dir, now);
        auto text = dir.getChildFile ("trial.dat").loadFileAsString();
        text = text.replace (juce::String (now), juce::String (now + 30 * day));
        dir.getChildFile ("trial.dat").replaceWithText (text);
        check (Licensing (dir).getStatus (product::amp).expired, "an edited trial file reads as ended");
    }

    // a clock set far back ends it too, a small clock difference does not
    Licensing::writeTrial (dir, now + 5 * day);
    check (Licensing (dir).getStatus (product::amp).expired, "a clock set back ends the trial");
    Licensing::writeTrial (dir, now + day / 2);
    check (! Licensing (dir).getStatus (product::amp).expired, "a few hours of clock difference are fine");

    dir.deleteRecursively();
    std::printf ("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
