#pragma once

#include <fmt/format.h>

#include <array>
#include <cstdint>
#include <random>
#include <string>

namespace sst::common::utils {

// RFC 4122 v4 (random) UUID. Used for match / recording / event-clip ids that
// double as filesystem path components — keep the canonical 8-4-4-4-12 hex form
// so directory listings remain human-scannable.
inline auto MakeUuidV4() -> std::string {
    thread_local std::mt19937_64 rng{std::random_device{}()};
    std::array<std::uint8_t, 16> bytes{};
    std::uint64_t hi = rng();
    std::uint64_t lo = rng();
    for (std::size_t i = 0; i < 8; ++i) {
        bytes[i] = static_cast<std::uint8_t>(hi >> (i * 8));
        bytes[i + 8] = static_cast<std::uint8_t>(lo >> (i * 8));
    }
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3F) | 0x80);

    return fmt::format(
        "{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-"
        "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8],
        bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
}

// Cryptographically-strong 128-bit random token (32 hex chars). Unlike
// MakeUuidV4 — which seeds an mt19937 PRNG once and is fine for human-scannable
// ids — every word here is pulled directly from std::random_device (/dev/urandom
// on Linux), so download bearer tokens are not predictable from observed ids.
inline auto MakeSecureToken() -> std::string {
    std::random_device rd;
    const std::array<std::uint32_t, 4> words{rd(), rd(), rd(), rd()};
    return fmt::format("{:08x}{:08x}{:08x}{:08x}", words[0], words[1], words[2], words[3]);
}

}  // namespace sst::common::utils
