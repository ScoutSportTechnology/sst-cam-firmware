#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string>

#include <fmt/format.h>

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

}  // namespace sst::common::utils
