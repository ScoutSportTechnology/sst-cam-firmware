#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace sst::adapters::control {

// Size of a broadcast gratuitous-ARP frame on Ethernet: a 14-byte Ethernet II
// header + a 28-byte ARP payload (ether_arp).
constexpr std::size_t kArpFrameLen = 42;

// Ethernet MAC address length (mirrors ETH_ALEN; named so it can appear in a
// std::array bound without tripping the magic-number gate).
constexpr std::size_t kMacLen = 6;

// Build a broadcast gratuitous-ARP *announcement* (RFC 5227) for `ip_be`
// (IPv4 address in network byte order) claimed by `mac`. The frame is an ARP
// REQUEST with sender IP == target IP == `ip_be`, sender hardware == `mac`,
// target hardware zeroed, destination MAC broadcast. Pure and allocation-free
// so the frame layout is unit-testable without a live radio.
[[nodiscard]] auto BuildArpAnnouncement(const std::array<std::uint8_t, kMacLen>& mac,
                                        std::uint32_t ip_be)
    -> std::array<std::uint8_t, kArpFrameLen>;

// Broadcast a few gratuitous-ARP announcements for `ip` (dotted-quad) on
// `iface` via a raw AF_PACKET socket, so a neighbour caching a stale/negative
// entry for `ip` *may* refresh it sooner (best-effort — a neighbour is free to
// ignore an unsolicited ARP for a FAILED entry; acceptance is arp_accept-gated
// and kernel-dependent, so this is NOT relied upon). Never throws and never
// blocks meaningfully: any failure (missing CAP_NET_RAW, unknown iface,
// unresolvable MAC, malformed IP) is logged and the call returns cleanly.
auto SendGratuitousArp(const std::string& iface, const std::string& ip_str) -> void;

}  // namespace sst::adapters::control
