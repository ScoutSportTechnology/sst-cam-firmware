#pragma once

#include <cstdint>

namespace sst::storage {

// Pre-flight check consulted by RecordingService before opening a new mp4
// (full-game recording start, event-clip trigger). Refuses the operation if
// the SSD is below the configured `min_free_bytes` threshold so the device
// fails fast at the BLE caller instead of crashing on a write deep into the
// pipeline.
//
// The check is one-shot at start; we don't watch disk space mid-recording —
// that's the plan-of-record posture per CLAUDE.md, kept here as a comment so
// future maintainers don't reach for a continuous monitor without thinking.
class IDiskGuard {
   public:
    virtual ~IDiskGuard() = default;

    // True if the configured path has at least `min_free_bytes` available
    // (or if no threshold is configured — guard is permissive by default).
    [[nodiscard]] virtual auto HasEnoughFreeSpace() const -> bool = 0;

    // Free bytes at the configured path. Returned for logging / BLE
    // responses; 0 on stat failure.
    [[nodiscard]] virtual auto FreeBytes() const -> std::uint64_t = 0;
};

}  // namespace sst::storage
