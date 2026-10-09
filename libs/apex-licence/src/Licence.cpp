#include "apex/licence/Licence.h"

extern "C"
{
#include "tweetnacl.h"
}

#include <random>
#include <vector>

// The development key pair: its private half is in dev/development-secret-key.txt,
// so a build that still carries it must never be sold.
#define APEX_DEVELOPMENT_PUBLIC_KEY "5755a626740752f71f4ede36c6a46324bca52935f5c5a0de1b9cab94d51ac8e8"
#ifndef APEX_LICENCE_PUBLIC_KEY
 #define APEX_LICENCE_PUBLIC_KEY APEX_DEVELOPMENT_PUBLIC_KEY
#endif

// TweetNaCl takes its randomness from here; only key pair generation uses it.
extern "C" void randombytes (unsigned char* out, unsigned long long size)
{
    std::random_device device;   // the OS source (getrandom, arc4random, RtlGenRandom)
    for (unsigned long long i = 0; i < size; ++i)
        out[i] = (unsigned char) (device() & 0xff);
}

namespace apex::licence
{

namespace
{
    constexpr char alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    constexpr std::string_view domain = "APEX-LICENCE-1";
    constexpr std::uint8_t formatVersion = 1;
    constexpr std::size_t maxOwner = 60, signatureSize = 64;

    std::string toBase32 (const std::vector<std::uint8_t>& bytes)
    {
        std::string out;
        std::uint32_t buffer = 0;
        int bits = 0;
        for (auto b : bytes)
        {
            buffer = (buffer << 8) | b;
            bits += 8;
            while (bits >= 5)
            {
                out += alphabet[(buffer >> (bits - 5)) & 31];
                bits -= 5;
            }
        }
        if (bits > 0)
            out += alphabet[(buffer << (5 - bits)) & 31];
        return out;
    }

    int base32Value (char c) noexcept
    {
        if (c >= 'a' && c <= 'z')
            c = (char) (c - 'a' + 'A');
        if (c == 'O') c = '0';
        if (c == 'I' || c == 'L') c = '1';
        for (int i = 0; i < 32; ++i)
            if (alphabet[i] == c)
                return i;
        return -1;
    }

    std::optional<std::vector<std::uint8_t>> fromBase32 (std::string_view text)
    {
        // keep only key characters; drop the APEX prefix
        std::string clean;
        for (char c : text)
            if (c != '-' && c != ' ' && c != '\n' && c != '\r' && c != '\t')
                clean += c;
        if (clean.size() >= 4)
        {
            std::string head = clean.substr (0, 4);
            for (auto& c : head)
                if (c >= 'a' && c <= 'z') c = (char) (c - 'a' + 'A');
            if (head == "APEX")
                clean.erase (0, 4);
        }

        std::vector<std::uint8_t> out;
        std::uint32_t buffer = 0;
        int bits = 0;
        for (char c : clean)
        {
            const int v = base32Value (c);
            if (v < 0)
                return std::nullopt;
            buffer = (buffer << 5) | (std::uint32_t) v;
            bits += 5;
            if (bits >= 8)
            {
                out.push_back ((std::uint8_t) ((buffer >> (bits - 8)) & 0xff));
                bits -= 8;
            }
        }
        return out;
    }

    std::vector<std::uint8_t> payloadOf (const Licence& l)
    {
        const auto owner = l.owner.substr (0, maxOwner);
        std::vector<std::uint8_t> p;
        p.push_back (formatVersion);
        p.push_back (l.products);
        p.push_back ((std::uint8_t) (l.issueDay & 0xff));
        p.push_back ((std::uint8_t) (l.issueDay >> 8));
        for (int i = 0; i < 4; ++i)
            p.push_back ((std::uint8_t) ((l.serial >> (8 * i)) & 0xff));
        p.push_back ((std::uint8_t) owner.size());
        p.insert (p.end(), owner.begin(), owner.end());
        return p;
    }

    std::vector<std::uint8_t> signedMessage (const std::vector<std::uint8_t>& payload)
    {
        std::vector<std::uint8_t> m (domain.begin(), domain.end());
        m.insert (m.end(), payload.begin(), payload.end());
        return m;
    }

    PublicKey parseBuiltInKey() noexcept
    {
        PublicKey key {};
        if (! fromHex (APEX_LICENCE_PUBLIC_KEY, key.data(), key.size()))
            fromHex (APEX_DEVELOPMENT_PUBLIC_KEY, key.data(), key.size());
        return key;
    }
}

std::optional<Licence> verify (std::string_view keyText, const PublicKey& publicKey)
{
    const auto bytes = fromBase32 (keyText);
    // version, products, day (2), serial (4), owner length: 9 bytes before the owner
    if (! bytes || bytes->size() < 9 + signatureSize)
        return std::nullopt;

    const auto& b = *bytes;
    const std::size_t ownerSize = b[8];
    const std::size_t payloadSize = 9 + ownerSize;
    // base32 pads the last character with zero bits; anything longer is not a key
    if (b[0] != formatVersion || ownerSize > maxOwner || b.size() != payloadSize + signatureSize)
        return std::nullopt;

    const std::vector<std::uint8_t> payload (b.begin(), b.begin() + (std::ptrdiff_t) payloadSize);
    const auto message = signedMessage (payload);

    std::vector<std::uint8_t> sm (b.begin() + (std::ptrdiff_t) payloadSize, b.end());
    sm.insert (sm.end(), message.begin(), message.end());
    std::vector<std::uint8_t> opened (sm.size());
    unsigned long long openedSize = 0;
    if (crypto_sign_open (opened.data(), &openedSize, sm.data(), sm.size(), publicKey.data()) != 0)
        return std::nullopt;

    Licence l;
    l.products = b[1];
    l.issueDay = (std::uint16_t) (b[2] | (b[3] << 8));
    l.serial = (std::uint32_t) b[4] | ((std::uint32_t) b[5] << 8) | ((std::uint32_t) b[6] << 16) | ((std::uint32_t) b[7] << 24);
    l.owner.assign (b.begin() + 9, b.begin() + (std::ptrdiff_t) payloadSize);
    return l;
}

std::string sign (const Licence& licence, const SecretKey& secretKey)
{
    const auto payload = payloadOf (licence);
    const auto message = signedMessage (payload);
    std::vector<std::uint8_t> sm (message.size() + signatureSize);
    unsigned long long smSize = 0;
    crypto_sign (sm.data(), &smSize, message.data(), message.size(), secretKey.data());

    auto bytes = payload;
    bytes.insert (bytes.end(), sm.begin(), sm.begin() + (std::ptrdiff_t) signatureSize);
    const auto body = toBase32 (bytes);

    std::string text = "APEX";
    for (std::size_t i = 0; i < body.size(); i += 5)
        text += "-" + body.substr (i, 5);
    return text;
}

void generateKeyPair (PublicKey& publicKey, SecretKey& secretKey)
{
    crypto_sign_keypair (publicKey.data(), secretKey.data());
}

const PublicKey& builtInPublicKey() noexcept
{
    static const PublicKey key = parseBuiltInKey();
    return key;
}

bool isDevelopmentBuild() noexcept
{
    PublicKey development {};
    fromHex (APEX_DEVELOPMENT_PUBLIC_KEY, development.data(), development.size());
    return builtInPublicKey() == development;
}

std::uint16_t dayNumber (std::int64_t unixSeconds) noexcept
{
    constexpr std::int64_t epoch = 1767225600;   // 2026-01-01T00:00:00Z
    const auto days = (unixSeconds - epoch) / 86400;
    return (std::uint16_t) (days < 0 ? 0 : (days > 65535 ? 65535 : days));
}

std::string toHex (const std::uint8_t* data, std::size_t size)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string s;
    for (std::size_t i = 0; i < size; ++i)
    {
        s += digits[data[i] >> 4];
        s += digits[data[i] & 15];
    }
    return s;
}

bool fromHex (std::string_view hex, std::uint8_t* out, std::size_t size)
{
    std::string clean;
    for (char c : hex)
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t')
            clean += c;
    if (clean.size() != 2 * size)
        return false;
    auto nibble = [] (char c) -> int
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < size; ++i)
    {
        const int hi = nibble (clean[2 * i]), lo = nibble (clean[2 * i + 1]);
        if (hi < 0 || lo < 0)
            return false;
        out[i] = (std::uint8_t) ((hi << 4) | lo);
    }
    return true;
}

} // namespace apex::licence
