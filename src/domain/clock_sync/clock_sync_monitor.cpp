#include "domain/clock_sync/clock_sync_monitor.hpp"

#include <spdlog/spdlog.h>
#include <sys/timex.h>

namespace sst::domain::clock_sync {

auto SystemClockSyncSource::IsSynced() const -> bool {
    struct timex timex_params {};
    timex_params.modes = 0;  // Just read the status
    int result = ntp_adjtime(&timex_params);
    if (result == -1) {
        // Fallback to adjtimex if ntp_adjtime is not available
        result = adjtimex(&timex_params);
    }
    if (result == -1) {
        // Both failed, assume not synced
        return false;
    }
    // STA_UNSYNC (0x0040) means the clock is not synchronized (NTP)
    constexpr int kStaUnsync = 0x0040;
    return (timex_params.status & kStaUnsync) == 0;
}

ClockSyncMonitor::ClockSyncMonitor(std::unique_ptr<IClockSyncSource> source,
                                   LogCallback log_callback)
    : source_(std::move(source)), log_callback_(std::move(log_callback)) {}

void ClockSyncMonitor::CheckAndLog() {
    bool synced = source_->IsSynced();
    if (first_check_) {
        // First call at startup - log the initial state
        log_callback_(fmt::format("clock: synced={}", synced ? "yes" : "no"));
        was_synced_ = synced;
        first_check_ = false;
    } else if (synced != was_synced_) {
        // Transition detected
        if (synced) {
            log_callback_("clock: synced=yes (wall-clock jump detected)");
        } else {
            log_callback_("clock: synced=no");
        }
        was_synced_ = synced;
    }
}

auto ClockSyncMonitor::IsSynced() const -> bool { return was_synced_; }

}  // namespace sst::domain::clock_sync