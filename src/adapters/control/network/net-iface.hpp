#pragma once

#include <string>

namespace sst::adapters::control {

// Interface discovery from /sys/class/net (no subprocess). Shared by the
// policy-routing adapter and the per-interface uplink probe so both agree on which
// NIC is the wired uplink and which is the WiFi-STA LAN.

// First ARPHRD_ETHER, non-wireless, non-virtual interface — the wired uplink NIC.
// Skips lo/docker/veth/br-/virbr/wl/p2p/rndis/usb and any iface with a /wireless
// subdir. Deterministic (sorted) so the pick is stable across boots. Empty if none.
auto DetectWiredIface() -> std::string;

// The base WiFi station interface — the first iface with a /wireless subdir whose
// name does NOT start with "p2p" (the "p2p-dev-*" virtual iface is skipped). Empty
// if none. This is the STA LAN interface the arbiter joins the phone's network on.
auto DetectWifiStaIface() -> std::string;

}  // namespace sst::adapters::control
