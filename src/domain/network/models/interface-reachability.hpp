#pragma once

#include <cstdint>

namespace sst::network {

// Real, per-interface internet reachability (R7). The uplink probe fills this from
// a lightweight bound TCP connect PER interface (Ethernet-WAN and WiFi-STA), so
// streaming egress can be steered onto whichever interface actually reaches the
// internet — not merely "a default route exists somewhere". Both false = the
// camera has no working uplink on any interface (streaming unavailable).
struct InterfaceReachability {
    bool ethernet_wan{false};  // the (different-subnet) Ethernet uplink reaches the internet
    bool wifi_sta{false};      // the WiFi-STA LAN interface reaches the internet

    // True iff at least one interface has a working uplink.
    [[nodiscard]] auto AnyReachable() const -> bool { return ethernet_wan || wifi_sta; }

    auto operator==(const InterfaceReachability&) const -> bool = default;
};

// Which interface streaming egress should use (R7). Ethernet-WAN is PREFERRED over
// WiFi-STA (a wired uplink is more stable and does not compete with the STA LAN
// link); kNone means neither interface is reachable, so streaming is unavailable.
enum class EgressInterface : std::uint8_t {
    kNone = 0,
    kEthernetWan = 1,
    kWifiSta = 2,
};

inline auto ToString(EgressInterface egress) -> const char* {
    switch (egress) {
        case EgressInterface::kNone:
            return "None";
        case EgressInterface::kEthernetWan:
            return "EthernetWan";
        case EgressInterface::kWifiSta:
            return "WifiSta";
    }
    return "Unknown";
}

// The egress-selection decision (R7): Ethernet-WAN preferred, else WiFi-STA, else
// none. Pure — the streaming handler consults it to gate a stream start (and, on
// device, to bind the RTMP egress onto the chosen interface).
inline auto SelectEgress(const InterfaceReachability& reachability) -> EgressInterface {
    if (reachability.ethernet_wan) {
        return EgressInterface::kEthernetWan;
    }
    if (reachability.wifi_sta) {
        return EgressInterface::kWifiSta;
    }
    return EgressInterface::kNone;
}

}  // namespace sst::network
