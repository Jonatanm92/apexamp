#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace apex::licence
{

/**
 * Apex licence keys
 * -----------------
 * A licence key is a small signed record: who owns it, which products it
 * unlocks, when it was issued and a serial number, signed with Ed25519. The
 * plugins only hold the public key, so they can check a key offline (no
 * server, no activation, works on a studio computer that never goes online)
 * but cannot make one. Keys are made with tools/apex_keygen and the private
 * key, which stays with the seller.
 *
 * As text a key reads APEX-XXXXX-XXXXX-... in Crockford base32: no I, L, O or
 * U, case, spaces, line breaks and dashes do not matter, so a key survives
 * being pasted out of an email.
 */

using PublicKey = std::array<std::uint8_t, 32>;
using SecretKey = std::array<std::uint8_t, 64>;   // Ed25519 seed + public key

namespace product
{
    constexpr std::uint8_t amp = 1, drop = 2, all = 0xff;
}

struct Licence
{
    std::uint8_t products = product::all;
    std::uint16_t issueDay = 0;     // days since 2026-01-01
    std::uint32_t serial = 0;
    std::string owner;              // name or email, up to 60 bytes of UTF-8

    bool unlocks (std::uint8_t p) const noexcept { return (products & p) != 0; }
};

/** Checks a key's signature and reads it; nothing if the text is not a valid key. */
std::optional<Licence> verify (std::string_view keyText, const PublicKey&);

/** Makes a key (seller side). */
std::string sign (const Licence&, const SecretKey&);

/** A new key pair from the operating system's random source (seller side). */
void generateKeyPair (PublicKey&, SecretKey&);

/** The public key built into this binary (APEX_LICENCE_PUBLIC_KEY, else the development key). */
const PublicKey& builtInPublicKey() noexcept;

/** True while the built-in key is the development key, whose private half is
    in the repository: anyone can make keys for such a build. Never ship one. */
bool isDevelopmentBuild() noexcept;

/** Days since 2026-01-01 for a Unix time in seconds. */
std::uint16_t dayNumber (std::int64_t unixSeconds) noexcept;

std::string toHex (const std::uint8_t* data, std::size_t size);
bool fromHex (std::string_view hex, std::uint8_t* out, std::size_t size);

} // namespace apex::licence
