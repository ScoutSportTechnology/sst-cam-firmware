#pragma once

#include <cstdint>

namespace sst::network {

// The REAL addressing mode the firmware-owned Ethernet interface is in — the
// single fact GetNetworkConfig reports so the app never sees a false "DHCP
// active" for a statically configured link (the DHCP-when-static bug, R12).
// The firmware owns the Ethernet config directly (ip/dhclient, no NetworkManager),
// so this is derived from ground truth: whether an address is bound, whether a
// dhclient lease is active, and the mode the firmware itself last applied.
//
//   kDown   — no carrier / no address: Ethernet is not up. `ethernet_dhcp` is
//             reported ABSENT for this state (unknown, not a defaulted DHCP).
//   kStatic — an address is bound with no active DHCP lease: statically
//             configured. Reports `ethernet_dhcp = false`.
//   kDhcp   — an address is bound via an active dhclient lease. Reports
//             `ethernet_dhcp = true`.
enum class EthernetMode : std::uint8_t {
    kDown = 0,
    kStatic = 1,
    kDhcp = 2,
};

inline auto ToString(EthernetMode mode) -> const char* {
    switch (mode) {
        case EthernetMode::kDown:
            return "Down";
        case EthernetMode::kStatic:
            return "Static";
        case EthernetMode::kDhcp:
            return "Dhcp";
    }
    return "Unknown";
}

}  // namespace sst::network
