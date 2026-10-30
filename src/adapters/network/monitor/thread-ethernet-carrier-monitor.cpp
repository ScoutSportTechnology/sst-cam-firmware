#include "adapters/network/monitor/thread-ethernet-carrier-monitor.hpp"

#include <mutex>
#include <utility>

namespace sst::adapters::network {

ThreadEthernetCarrierMonitor::ThreadEthernetCarrierMonitor(std::chrono::milliseconds interval)
    : interval_(interval) {}

ThreadEthernetCarrierMonitor::~ThreadEthernetCarrierMonitor() { Stop(); }

auto ThreadEthernetCarrierMonitor::Start(std::function<void()> on_poll) -> void {
    // Assign thread_ while holding run_mtx_ so a concurrent Stop() can never see
    // running_==true with an unassigned thread_ (telemetry-poller learning: the
    // Loop thread would be joinable-but-never-joined -> std::terminate).
    const std::lock_guard lock(run_mtx_);
    if (running_) {
        return;
    }
    on_poll_ = std::move(on_poll);
    running_ = true;
    thread_ = std::thread([this] { Loop(); });
}

auto ThreadEthernetCarrierMonitor::Stop() -> void {
    {
        const std::lock_guard lock(run_mtx_);
        if (!running_) {
            return;
        }
        running_ = false;
    }
    run_cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

auto ThreadEthernetCarrierMonitor::Loop() -> void {
    std::unique_lock lock(run_mtx_);
    while (running_) {
        // Wait the interval FIRST (wake immediately on Stop) so the very first tick
        // does not race the rest of boot wiring; the arbiter has already probed
        // Ethernet once during initial arbitration.
        if (run_cv_.wait_for(lock, interval_, [this] { return !running_; })) {
            break;
        }
        // Snapshot the callback under the lock, then invoke it WITHOUT the lock: it
        // probes `ip`/sysfs and (on a real transition) re-arbitrates, which must not
        // hold this monitor's lock across a fork or a re-entrant Stop().
        auto poll = on_poll_;
        lock.unlock();
        if (poll) {
            poll();
        }
        lock.lock();
    }
}

}  // namespace sst::adapters::network
