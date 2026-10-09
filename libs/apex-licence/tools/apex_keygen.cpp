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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #define NOMINMAX
 #include <windows.h>
#endif

using namespace apex::licence;

namespace
{
    // Arguments and paths are UTF-8 on every platform, so a buyer called
    // "Ångström" gets the same key on Windows as elsewhere.
    std::filesystem::path pathOf (const std::string& utf8)
    {
       #ifdef _WIN32
        const int n = MultiByteToWideChar (CP_UTF8, 0, utf8.c_str(), (int) utf8.size(), nullptr, 0);
        std::wstring wide ((size_t) n, L'\0');
        MultiByteToWideChar (CP_UTF8, 0, utf8.c_str(), (int) utf8.size(), wide.data(), n);
        return std::filesystem::path (wide);
       #else
        return std::filesystem::path (utf8);
       #endif
    }

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
        std::ifstream in (pathOf (path));
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

    using Args = std::vector<std::string>;

    std::string option (const Args& args, const char* name, const std::string& fallback = {})
    {
        for (size_t i = 1; i + 1 < args.size(); ++i)
            if (args[i] == name)
                return args[i + 1];
        return fallback;
    }

    std::uint32_t randomSerial()
    {
        std::random_device device;
        return (std::uint32_t) device();
    }
}

int run (const Args& args)
{
    if (args.size() < 3)
        return usage();
    const std::string command = args[1];

    if (command == "keypair")
    {
        const std::string path = args[2];
        if (std::ifstream (pathOf (path)).good())
        {
            std::cerr << path << " already exists; not overwriting a secret key\n";
            return 1;
        }
        PublicKey pub;
        SecretKey sec;
        generateKeyPair (pub, sec);
        std::ofstream out (pathOf (path));
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
        if (! readSecret (args[2], sec))
            return 1;
        Licence l;
        l.owner = option (args, "--owner");
        if (l.owner.empty())
            return usage();
        l.products = parseProducts (option (args, "--products", "all"));
        const auto serial = option (args, "--serial");
        l.serial = serial.empty() ? randomSerial() : (std::uint32_t) std::stoul (serial);
        l.issueDay = today();
        std::cout << sign (l, sec) << "\n";
        return 0;
    }

    if (command == "batch")
    {
        if (args.size() < 4)
            return usage();
        SecretKey sec;
        if (! readSecret (args[2], sec))
            return 1;
        const int count = std::stoi (args[3]);
        Licence l;
        l.products = parseProducts (option (args, "--products", "all"));
        const auto first = option (args, "--first-serial");
        l.serial = first.empty() ? randomSerial() : (std::uint32_t) std::stoul (first);
        l.issueDay = today();
        for (int i = 0; i < count; ++i, ++l.serial)
            std::cout << sign (l, sec) << "\n";
        return 0;
    }

    if (command == "check")
    {
        PublicKey pub = builtInPublicKey();
        const auto hex = option (args, "--public");
        if (! hex.empty() && ! fromHex (hex, pub.data(), pub.size()))
        {
            std::cerr << "bad public key\n";
            return 2;
        }
        const auto l = verify (args[2], pub);
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

#ifdef _WIN32
// Windows hands main() its arguments in the ANSI code page; take them as UTF-16 and convert
int wmain (int argc, wchar_t** argv)
{
    SetConsoleOutputCP (CP_UTF8);
    Args args;
    for (int i = 0; i < argc; ++i)
    {
        const int n = WideCharToMultiByte (CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string utf8 ((size_t) (n > 0 ? n - 1 : 0), '\0');
        WideCharToMultiByte (CP_UTF8, 0, argv[i], -1, utf8.data(), n, nullptr, nullptr);
        args.push_back (std::move (utf8));
    }
    return run (args);
}
#else
int main (int argc, char** argv)
{
    return run (Args (argv, argv + argc));
}
#endif
