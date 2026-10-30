#pragma once

#include "domain/network/models/interface-reachability.hpp"

namespace sst::streaming {

// Read-only probe for whether the camera currently has an internet uplink —
// i.e. a default route to reach the cloud (RTMP). The WiFi-Direct GO is
// link-local (192.168.49.0/24, no default route on the camera), so a default
// route exists ONLY when an uplink (ethernet / wifi-STA) is up. The streaming
// START path consults this so a cloud stream with no uplink fails with a clear
// "configure a network uplink" message instead of an opaque rtmp connect error.
class IUplinkProbe {
   public:
    IUplinkProbe() = default;
    virtual ~IUplinkProbe() = default;

    IUplinkProbe(const IUplinkProbe&) = delete;
    auto operator=(const IUplinkProbe&) -> IUplinkProbe& = delete;
    IUplinkProbe(IUplinkProbe&&) = delete;
    auto operator=(IUplinkProbe&&) -> IUplinkProbe& = delete;

    // True when the camera has a default route (an active internet uplink).
    [[nodiscard]] virtual auto HasInternetUplink() -> bool = 0;

    // Real PER-INTERFACE internet reachability (R7). The real adapter refreshes
    // this on an owned background poll thread into a cache (never an inline fork on
    // the BLE dispatcher — telemetry-poller learning), so the streaming handler
    // reads it instantly. Egress selection prefers Ethernet-WAN then STA. The
    // default derives from HasInternetUplink() so a presence-only probe still
    // works; the real adapter OVERRIDES it with the per-interface cache.
    [[nodiscard]] virtual auto ReachableInterfaces() -> sst::network::InterfaceReachability {
        return HasInternetUplink()
                   ? sst::network::InterfaceReachability{.ethernet_wan = true, .wifi_sta = false}
                   : sst::network::InterfaceReachability{};
    }
};

}  // namespace sst::streaming
