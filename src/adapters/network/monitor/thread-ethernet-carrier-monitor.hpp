#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <thread>

#include "app/network/ports/ethernet-carrier-monitor.hpp"

namespace sst::adapters::network {

// Owns the Ethernet netlink subscription and a two-second fallback level check
// on the existing monitor thread; callbacks run off the BLE dispatcher.
class ThreadEthernetCarrierMonitor final : public sst::network::IEthernetCarrierMonitor {
   public:
    static constexpr std::chrono::seconds kDefaultInterval{2};

    struct EventSource {
        int fd;
        unsigned int interface_index;
    };
    explicit ThreadEthernetCarrierMonitor(std::chrono::milliseconds interval = kDefaultInterval);
    ThreadEthernetCarrierMonitor(std::chrono::milliseconds interval, EventSource source);
    ~ThreadEthernetCarrierMonitor() override;

    ThreadEthernetCarrierMonitor(const ThreadEthernetCarrierMonitor&) = delete;
    auto operator=(const ThreadEthernetCarrierMonitor&) -> ThreadEthernetCarrierMonitor& = delete;
    ThreadEthernetCarrierMonitor(ThreadEthernetCarrierMonitor&&) = delete;
    auto operator=(ThreadEthernetCarrierMonitor&&) -> ThreadEthernetCarrierMonitor& = delete;

    auto Start(std::function<void()> on_poll) -> void override;
    auto Stop() -> void override;

   private:
    auto Loop() -> void;
    static auto OpenSocket() -> int;
    static auto ReadEvent(EventSource source) -> bool;

    const std::chrono::milliseconds interval_;
    const int event_fd_;
    const unsigned int interface_index_;
    std::function<void()> on_poll_;

    std::thread thread_;
    std::mutex run_mtx_;
    bool running_{false};
};

}  // namespace sst::adapters::network
