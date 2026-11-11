#pragma once

#include <functional>
#include <memory>

namespace sst::domain::clock_sync {

// Interface for checking clock synchronization status.
// Allows injecting a fake source for testing.
class IClockSyncSource {
public:
    virtual ~IClockSyncSource() = default;
    // Returns true if the system clock is synchronized, false otherwise.
    virtual bool IsSynced() const = 0;
};

// Default implementation using ntp_adjtime/adjtimex.
class SystemClockSyncSource final : public IClockSyncSource {
public:
    bool IsSynced() const override;
};

// Monitors clock synchronization state and logs changes.
// Uses an injected clock sync source for testability.
class ClockSyncMonitor {
public:
    using LogCallback = std::function<void(const std::string&)>;

    // Constructs a monitor with the given clock sync source and log callback.
    explicit ClockSyncMonitor(
        std::unique_ptr<IClockSyncSource> source,
        LogCallback log_callback);

    // Checks the clock sync state and logs if it changed.
    // Call this periodically (e.g., every second) to detect transitions.
    void CheckAndLog();

    // Returns the last known sync state (from the most recent CheckAndLog call).
    bool IsSynced() const;

private:
    std::unique_ptr<IClockSyncSource> source_;
    LogCallback log_callback_;
    bool was_synced_ = false;
    bool first_check_ = true;
};

}  // namespace sst::domain::clock_sync