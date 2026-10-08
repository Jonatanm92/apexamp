// apex_keygen: makes and checks Apex licence keys (seller side).
//
//   apex_keygen keypair <secret-key-file>
//       New key pair. Writes the secret key (keep it offline and backed up:
//       without it no new keys can be made for the builds that carry its
//       public key) and prints the public key for APEX_LICENCE_PUBLIC_KEY.
//
//   apex_keygen issue <secret-key-file> --owner "Name or email" [--products amp|drop|all] [--serial N]
//       One key for one buyer; the owner is shown in the plugin.
//
//   apex_keygen batch <secret-key-file> <count> [--products amp|drop|all] [--first-serial N]
//       Keys without an owner, one per line: a key list to upload to a store
//       that hands out one key per sale.
//
//   apex_keygen check <key> [--public <hex>]
//       Reads a key and checks it against a public key (default: this build's).
#include "apex/licence/Licence.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

using namespace apex::licence;

namespace
{
    int usage()
    {
        std::cerr << "usage:\n"
                     "  apex_keygen keypair <secret-key-file>\n"
                     "  apex_keygen issue <secret-key-file> --owner \"Name\" [--products amp|drop|all] [--serial N]\n"
                     "  apex_keygen batch <secret-key-file> <count> [--products amp|drop|all] [--first-serial N]\n"
                     "  apex_keygen check <key> [--public <hex>]\n";
        return 2;
    }

    bool readSecret (const std::string& path, SecretKey& key)
    {
        std::ifstream in (path);
        std::stringstream text;
        text << in.rdbuf();
        if (! in || ! fromHex (text.str(), key.data(), key.size()))
        {
            std::cerr << "cannot read a secret key from " << path << "\n";
            return false;
        }
        return true;
    }

    std::uint16_t today()
    {
        const auto now = std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now().time_since_epoch()).count();
        return dayNumber ((std::int64_t) now);
    }

    std::uint8_t parseProducts (const std::string& s)
    {
        if (s == "amp") return product::amp;
        if (s == "drop") return product::drop;
        return product::all;
    }

    std::string option (int argc, char** argv, const char* name, const std::string& fallback = {})
    {
        for (int i = 1; i + 1 < argc; ++i)
            if (std::strcmp (argv[i], name) == 0)
                return argv[i + 1];
        return fallback;
    }

    std::uint32_t randomSerial()
    {
        std::random_device device;
        return (std::uint32_t) device();
    }
}

int main (int argc, char** argv)
{
    if (argc < 3)
        return usage();
    const std::string command = argv[1];

    if (command == "keypair")
    {
        const std::string path = argv[2];
        if (std::ifstream (path).good())
        {
            std::cerr << path << " already exists; not overwriting a secret key\n";
            return 1;
        }
        PublicKey pub;
        SecretKey sec;
        generateKeyPair (pub, sec);
        std::ofstream out (path);
        out << toHex (sec.data(), sec.size()) << "\n";
        if (! out)
        {
            std::cerr << "cannot write " << path << "\n";
            return 1;
        }
        std::cout << "secret key written to " << path << " (keep it offline, back it up, never commit it)\n"
                  << "public key: " << toHex (pub.data(), pub.size()) << "\n"
                  << "build with: -DAPEX_LICENCE_PUBLIC_KEY=" << toHex (pub.data(), pub.size()) << "\n";
        return 0;
    }

    if (command == "issue")
    {
        SecretKey sec;
        if (! readSecret (argv[2], sec))
            return 1;
        Licence l;
        l.owner = option (argc, argv, "--owner");
        if (l.owner.empty())
            return usage();
        l.products = parseProducts (option (argc, argv, "--products", "all"));
        const auto serial = option (argc, argv, "--serial");
        l.serial = serial.empty() ? randomSerial() : (std::uint32_t) std::stoul (serial);
        l.issueDay = today();
        std::cout << sign (l, sec) << "\n";
        return 0;
    }

    if (command == "batch")
    {
        if (argc < 4)
            return usage();
        SecretKey sec;
        if (! readSecret (argv[2], sec))
            return 1;
        const int count = std::stoi (argv[3]);
        Licence l;
        l.products = parseProducts (option (argc, argv, "--products", "all"));
        const auto first = option (argc, argv, "--first-serial");
        l.serial = first.empty() ? randomSerial() : (std::uint32_t) std::stoul (first);
        l.issueDay = today();
        for (int i = 0; i < count; ++i, ++l.serial)
            std::cout << sign (l, sec) << "\n";
        return 0;
    }

    if (command == "check")
    {
        PublicKey pub = builtInPublicKey();
        const auto hex = option (argc, argv, "--public");
        if (! hex.empty() && ! fromHex (hex, pub.data(), pub.size()))
        {
            std::cerr << "bad public key\n";
            return 2;
        }
        const auto l = verify (argv[2], pub);
        if (! l)
        {
            std::cout << "INVALID\n";
            return 1;
        }
        std::cout << "valid: owner \"" << l->owner << "\", products " << (int) l->products << ", serial " << l->serial
                  << ", issued day " << l->issueDay << "\n";
        return 0;
    }

    return usage();
}
