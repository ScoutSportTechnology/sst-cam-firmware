#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include "app/network/ports/ethernet-carrier-monitor.hpp"

namespace sst::adapters::network {

// Poll-thread IEthernetCarrierMonitor (R20). Owns one background thread that
// invokes the registered callback every `interval`, INDEPENDENT of the WiFi radio
// mode, so the arbiter re-checks the Ethernet carrier and re-arbitrates on a
// hotplug/unplug even while kP2pOnly or with LanPath=kEthernet committed. The
// callback (which probes `ip`/sysfs) runs on THIS thread, never the BLE
// dispatcher (telemetry-poller learning); the arbiter de-dupes on the observed
// carrier, so a poke with no real change is a cheap no-op.
class ThreadEthernetCarrierMonitor final : public sst::network::IEthernetCarrierMonitor {
   public:
    // Coarse by design: an Ethernet hotplug is not latency-critical, and a tight
    // poll would fork `ip` needlessly. Metal-tunable.
    static constexpr std::chrono::seconds kDefaultInterval{5};

    explicit ThreadEthernetCarrierMonitor(std::chrono::milliseconds interval = kDefaultInterval);
    ~ThreadEthernetCarrierMonitor() override;

    ThreadEthernetCarrierMonitor(const ThreadEthernetCarrierMonitor&) = delete;
    auto operator=(const ThreadEthernetCarrierMonitor&) -> ThreadEthernetCarrierMonitor& = delete;
    ThreadEthernetCarrierMonitor(ThreadEthernetCarrierMonitor&&) = delete;
    auto operator=(ThreadEthernetCarrierMonitor&&) -> ThreadEthernetCarrierMonitor& = delete;

    auto Start(std::function<void()> on_poll) -> void override;
    auto Stop() -> void override;

   private:
    auto Loop() -> void;

    const std::chrono::milliseconds interval_;
    std::function<void()> on_poll_;

    std::thread thread_;
    std::mutex run_mtx_;
    std::condition_variable run_cv_;
    bool running_{false};
};

}  // namespace sst::adapters::network
