#pragma once

#include <string>

namespace sst::adapters::control {

// Interface discovery from /sys/class/net (no subprocess). The SINGLE source of
// truth for which NIC is the wired uplink and which is the WiFi-STA LAN — shared by
// the policy-routing adapter, the per-interface uplink probe, and the Ethernet
// configurator so they can never disagree (a second, weaker copy once picked can0
// for the firewall while the configurator picked the real NIC).

// First ARPHRD_ETHER, non-wireless, non-virtual interface — the wired uplink NIC.
// Requires /sys/class/net/<if>/type == 1: on a Jetson the alphabetically-first
// netdev is can0 (ARPHRD_CAN), so dropping that gate silently retargets the egress
// firewall and the reachability probe onto a CAN bus. Also skips
// lo/docker/veth/br-/virbr/wl/p2p/rndis/usb/l4tbr and anything with a /wireless
// subdir. Deterministic (sorted) so the pick is stable across boots. Empty if none.
auto DetectWiredIface() -> std::string;

// The base WiFi station interface — the first iface with a /wireless subdir whose
// name does NOT start with "p2p" (the "p2p-dev-*" virtual iface is skipped). Empty
// if none. This is the STA LAN interface the arbiter joins the phone's network on.
auto DetectWifiStaIface() -> std::string;

}  // namespace sst::adapters::control
