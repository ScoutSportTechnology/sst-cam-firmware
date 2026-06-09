#pragma once

#include <cstdint>

namespace sst::control {

// Point-in-time device health snapshot, sourced from the OS / hardware sensors
// and mapped into the proto DeviceTelemetry response. Hardware-specific reads
// (SoC temperature, battery) are best-effort and may be zero off-device.
struct SystemStats {
    std::uint64_t storage_free_bytes{0};
    std::uint64_t storage_total_bytes{0};
    float temp_celsius{0.0F};
    float ram_used_pct{0.0F};
    float cpu_used_pct{0.0F};
    std::uint64_t uptime_seconds{0};
    std::uint32_t battery_level_pct{0};
};

}  // namespace sst::control
