#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "app/streaming/ports/uplink-probe.hpp"
#include "domain/network/models/interface-reachability.hpp"

namespace sst::adapters::control {

// Pure parser (unit-testable, no I/O): true iff `ip route show default` output
// contains at least one default route. A camera default route implies an active
// uplink — the WiFi-Direct GO is link-local and installs none on the camera.
auto HasDefaultRoute(const std::string& ip_route_default_output) -> bool;

// The REAL per-interface reachability sample (R7, hardware-bound): binds a
// lightweight TCP connect to EACH interface (Ethernet-WAN + WiFi-STA) via
// SO_BINDTODEVICE and reports which ones reach the internet. SO_BINDTODEVICE is
// chosen over a source-routed connect so the result is INDEPENDENT of the live
// routing table (the arbiter installs/removes uplink policy routing concurrently,
// so a route-table-dependent probe would race it). Verified on-device; in the
// cross-compile container both connects fail (no uplink), so it returns all-false
// without throwing.
auto ProbeInterfaceReachability() -> sst::network::InterfaceReachability;

// Per-interface uplink probe backed by an OWNED background poll thread (R7 +
// telemetry-poller learning). The per-interface TCP connects are slow and MUST
// NOT run inline on the single BLE dispatcher — a hung connect would stall every
// BLE command. So the probe samples on its own thread into a cache; the streaming
// handler reads `ReachableInterfaces()` / `HasInternetUplink()` instantly.
//
// Start() before use; Stop()/destruction joins the thread. Getters are safe from
// any thread. Start()/Stop() are idempotent (single controlling thread).
class IpRouteUplinkProbe final : public sst::streaming::IUplinkProbe {
   public:
    static constexpr std::chrono::seconds kDefaultInterval{5};
    using ReachabilitySampler = std::function<sst::network::InterfaceReachability()>;

    // `sampler` defaults to the real per-interface probe; tests inject a fake to
    // drive the cache deterministically without touching the network. `interval`
    // accepts milliseconds so tests can poll fast.
    explicit IpRouteUplinkProbe(ReachabilitySampler sampler = ProbeInterfaceReachability,
                                std::chrono::milliseconds interval = kDefaultInterval);
    ~IpRouteUplinkProbe() override;

    IpRouteUplinkProbe(const IpRouteUplinkProbe&) = delete;
    auto operator=(const IpRouteUplinkProbe&) -> IpRouteUplinkProbe& = delete;
    IpRouteUplinkProbe(IpRouteUplinkProbe&&) = delete;
    auto operator=(IpRouteUplinkProbe&&) -> IpRouteUplinkProbe& = delete;

    // Starts the poll thread; its first pass samples immediately (on the thread, so
    // Start() never blocks the caller on a slow connect). Idempotent.
    auto Start() -> void;
    // Signals and joins the poll thread. Idempotent; also called by the dtor.
    auto Stop() -> void;

    // Cache reads — instant, never fork/connect on the caller's thread.
    [[nodiscard]] auto HasInternetUplink() -> bool override {
        return ReachableInterfaces().AnyReachable();
    }
    [[nodiscard]] auto ReachableInterfaces() -> sst::network::InterfaceReachability override;

   private:
    auto SampleOnce() -> void;
    auto Loop() -> void;

    ReachabilitySampler sampler_;
    std::chrono::milliseconds interval_;

    mutable std::mutex cache_mtx_;
    sst::network::InterfaceReachability cache_;

    std::thread thread_;
    std::mutex run_mtx_;
    std::condition_variable run_cv_;
    bool running_{false};
};

}  // namespace sst::adapters::control
