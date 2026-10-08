// Licence keys: Ed25519 against RFC 8032, sign / verify round trips, tampering,
// forgiving parsing and the development key.
#include "apex/licence/Licence.h"

extern "C"
{
#include "tweetnacl.h"
}

#include <cstdio>
#include <cstring>
#include <string>

using namespace apex::licence;

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

    SecretKey secretFrom (const char* hex)
    {
        SecretKey k {};
        fromHex (hex, k.data(), k.size());
        return k;
    }

    PublicKey publicOf (const SecretKey& s)
    {
        PublicKey p {};
        std::memcpy (p.data(), s.data() + 32, 32);
        return p;
    }
}

int main()
{
    // RFC 8032, Ed25519 test 1 (empty message)
    {
        const auto sk = secretFrom ("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"
                                    "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
        unsigned char sm[64];
        unsigned long long smlen = 0;
        crypto_sign (sm, &smlen, nullptr, 0, sk.data());
        check (toHex (sm, 64) == "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b",
               "RFC 8032 test 1 signature");
    }

    // a fresh key pair signs and verifies
    PublicKey pub;
    SecretKey sec;
    generateKeyPair (pub, sec);
    check (publicOf (sec) == pub, "secret key carries its public key");

    Licence l;
    l.owner = "Test Buyer <buyer@example.com>";
    l.products = product::amp;
    l.serial = 123456789u;
    l.issueDay = dayNumber (1790000000);   // 2026-09-21
    const auto key = sign (l, sec);
    std::printf ("licence key (%zu chars): %s\n", key.size(), key.c_str());

    {
        const auto got = verify (key, pub);
        check (got.has_value(), "valid key verifies");
        if (got)
        {
            check (got->owner == l.owner && got->serial == l.serial && got->issueDay == l.issueDay && got->products == l.products,
                   "fields read back");
            check (got->unlocks (product::amp) && ! got->unlocks (product::drop), "product mask");
        }
    }

    // forgiving to how it was pasted
    {
        std::string messy;
        for (char c : key)
            messy += (char) (c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        messy = "  " + messy.substr (0, 40) + "\r\n" + messy.substr (40) + "\n";
        check (verify (messy, pub).has_value(), "lower case and line breaks");

        std::string noDashes;
        for (char c : key)
            if (c != '-') noDashes += c;
        check (verify (noDashes, pub).has_value(), "without dashes");

        std::string misread = key;
        for (auto& c : misread)
            if (c == '0') { c = 'O'; break; }
        check (verify (misread, pub).has_value(), "O read as 0");
    }

    // tampering and wrong keys fail
    {
        int caught = 0, tried = 0;
        for (std::size_t i = 5; i < key.size(); i += 3)
        {
            if (key[i] == '-')
                continue;
            auto bad = key;
            bad[i] = bad[i] == 'Z' ? 'Y' : 'Z';
            ++tried;
            if (! verify (bad, pub).has_value())
                ++caught;
        }
        std::printf ("  tampered keys rejected: %d / %d\n", caught, tried);
        check (caught == tried, "every changed character is rejected");

        PublicKey otherPub;
        SecretKey otherSec;
        generateKeyPair (otherPub, otherSec);
        check (! verify (key, otherPub).has_value(), "another public key rejects it");
        check (! verify (sign (l, otherSec), pub).has_value(), "a key from another secret is rejected");
        check (! verify (key.substr (0, key.size() - 6), pub).has_value(), "truncated key");
        check (! verify (key + "-ABCDE", pub).has_value(), "extended key");
        check (! verify ("", pub).has_value() && ! verify ("hello world", pub).has_value(), "garbage");
    }

    // owner limits and unicode
    {
        Licence u;
        u.owner = "\xc3\x85ngstr\xc3\xb6m \xc3\x96lander";   // Ångström Ölander
        const auto got = verify (sign (u, sec), pub);
        check (got && got->owner == u.owner, "UTF-8 owner");

        Licence longOwner;
        longOwner.owner = std::string (100, 'x');
        const auto cut = verify (sign (longOwner, sec), pub);
        check (cut && cut->owner.size() == 60, "owner cut to 60 bytes");

        Licence anonymous;
        anonymous.serial = 7;
        const auto anon = verify (sign (anonymous, sec), pub);
        check (anon && anon->owner.empty() && anon->serial == 7, "key without an owner");
    }

    // the development key in the repository matches the built-in development public key
    {
        const auto dev = secretFrom ("97b3847396aead6bd3d00fb4a5c3800afbb474ce1785c9fa983054d6c14785ec"
                                     "5755a626740752f71f4ede36c6a46324bca52935f5c5a0de1b9cab94d51ac8e8");
        const auto devKey = sign (l, dev);
        if (isDevelopmentBuild())
            check (verify (devKey, builtInPublicKey()).has_value(), "development key verifies in a development build");
        else
            check (! verify (devKey, builtInPublicKey()).has_value(), "development keys rejected in a release build");
        std::printf ("  built-in key: %s\n", isDevelopmentBuild() ? "development" : "release");
    }

    check (dayNumber (1767225600) == 0 && dayNumber (1767225600 + 86400 * 10 + 5) == 10 && dayNumber (0) == 0, "day numbers");

    std::printf ("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
