#pragma once

#include <functional>

namespace sst::network {

// An always-on Ethernet carrier monitor (R20 mode-independent monitor). The
// connectivity arbiter's existing carrier watchdog reuses the idle
// ISelfRevertTimer and only arms in kStaOnly, so it CANNOT see an Ethernet
// hotplug while in kP2pOnly, nor an Ethernet unplug while LanPath=kEthernet is
// committed (the radio is not kStaOnly). This seam runs for the whole process
// lifetime, INDEPENDENT of the radio mode, and pokes the arbiter to re-run
// LAN-path arbitration whenever the Ethernet carrier could have changed.
//
// The real adapter owns a single poll thread (off the BLE dispatcher — the poke
// callback probes `ip`/`sysfs`, which must never fork inline on the dispatcher;
// telemetry-poller learning). A test double drives the callback synchronously
// (like ISelfRevertTimer::Fire) so the LAN-path x carrier transition matrix is
// deterministic.
//
// Contract:
//   - Start registers the callback and begins polling. The callback fires on the
//     monitor's own thread, so the arbiter's callback must take its own lock and
//     tolerate being invoked with no actual change (it de-dupes on the observed
//     carrier). Idempotent — a second Start with the monitor already running is a
//     no-op.
//   - Stop halts polling and joins the thread; safe if never started. The arbiter
//     calls Stop in its destructor so no callback fires into a torn-down arbiter.
class IEthernetCarrierMonitor {
   public:
    IEthernetCarrierMonitor() = default;
    virtual ~IEthernetCarrierMonitor() = default;

    IEthernetCarrierMonitor(const IEthernetCarrierMonitor&) = delete;
    auto operator=(const IEthernetCarrierMonitor&) -> IEthernetCarrierMonitor& = delete;
    IEthernetCarrierMonitor(IEthernetCarrierMonitor&&) = delete;
    auto operator=(IEthernetCarrierMonitor&&) -> IEthernetCarrierMonitor& = delete;

    virtual auto Start(std::function<void()> on_poll) -> void = 0;
    virtual auto Stop() -> void = 0;
};

}  // namespace sst::network
