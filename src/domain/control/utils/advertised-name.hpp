#pragma once

#include <fmt/format.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "domain/control/models/bootstrap-config.hpp"

namespace sst::control {

// Format the BLE advertised name as `sst-cam-NNNN` with the unit number zero-
// padded to 4 digits (R2a). Numbers beyond 4 digits wrap mod 10000 so the name
// always matches the contract's fixed-width pattern.
inline auto MakeAdvertisedName(std::uint32_t unit_number) -> std::string {
    return fmt::format("{}{:04}", BootstrapDefaults::kBleNamePrefix, unit_number % 10000U);
}

// Derive the unit number from a device serial number by reading its numeric
// value (leading zeros and any non-digit prefix/suffix are tolerated, e.g.
// "00000042" -> 42, "SN-7" -> 7). An empty / digit-less serial yields 0.
inline auto DeriveUnitNumber(std::string_view serial_number) -> std::uint32_t {
    std::uint64_t value = 0;
    bool saw_digit = false;
    for (const char ch : serial_number) {
        if (ch >= '0' && ch <= '9') {
            saw_digit = true;
            value = (value * 10U) + static_cast<std::uint32_t>(ch - '0');
            // Bound growth; only the low 4 digits matter for the name.
            value %= 1'000'000'000ULL;
        } else if (saw_digit) {
            // Stop at the first non-digit after a run of digits (suffix noise).
            break;
        }
    }
    return static_cast<std::uint32_t>(value);
}

}  // namespace sst::control
