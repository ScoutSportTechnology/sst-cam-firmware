#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/network/models/ethernet-mode.hpp"

namespace sst::adapters::control {

// Pure, exec-free helpers for the firmware-owned Ethernet configurator: strict
// input validation (BLE-supplied IP/gateway/DNS is untrusted), argv construction
// for `ip`, resolv.conf content formatting, DHCP-offer lease parsing, and
// static-vs-DHCP mode derivation. Factored out of the fork/exec adapter so the
// injection/content-safety surface + the reporting logic are unit-testable
// without touching `ip`/`dhclient` (verified on-device). Each argv builder
// returns an empty vector on invalid input so the caller fails fast instead of
// exec'ing a half-formed — or attacker-shaped — command.

// True iff `text` is a STRICT dotted-quad IPv4 literal (four 0-255 octets, no
// leading zeros beyond a lone "0", no sign, no whitespace, no embedded newline or
// any other character). This is the injection/content-safety gate for every
// BLE-supplied address/gateway/DNS token before it reaches an `ip` argv or the
// /etc/resolv.conf writer.
auto IsIpv4Literal(const std::string& text) -> bool;

// True iff `text` is a strict IPv4 CIDR "a.b.c.d/p" with p in [0,32].
auto IsIpv4Cidr(const std::string& text) -> bool;

// Contiguous-prefix length of a dotted-quad IPv4 netmask (e.g. 255.255.255.0 →
// 24). nullopt if `mask` is not a valid IPv4 literal or not a contiguous mask.
auto MaskToPrefix(const std::string& mask) -> std::optional<int>;

// The /etc/resolv.conf body the firmware writes for a static config: a single
// "nameserver <dns>\n" line, or empty when `dns` is empty. PRECONDITION: `dns`
// must already be IsIpv4Literal-validated by the caller — this formatter must
// never receive an untrusted string (a smuggled newline/directive would inject a
// second resolv.conf line).
auto BuildResolvConf(const std::string& dns) -> std::string;

// `ip addr replace <cidr> dev <iface>` — idempotent static assignment. Empty on
// an invalid CIDR or empty iface.
auto BuildEthAddrReplaceArgv(const std::string& cidr,
                             const std::string& iface) -> std::vector<std::string>;

// `ip addr del <cidr> dev <iface>` — remove ONE specific address. Used instead of
// `ip addr flush` so teardown can only ever remove the address the firmware itself
// applied, leaving an operator's out-of-band address (e.g. a static management IP
// set before the firmware took the NIC over) untouched. Empty on an invalid CIDR.
auto BuildEthAddrDelArgv(const std::string& cidr,
                         const std::string& iface) -> std::vector<std::string>;

// `ip route replace default via <gateway> dev <iface>` — idempotent default
// route. Empty on an invalid gateway literal or empty iface.
auto BuildEthRouteReplaceArgv(const std::string& gateway,
                              const std::string& iface) -> std::vector<std::string>;

// Extract the OFFERED address as CIDR from a dhclient leases-file body (as
// produced by a no-op `-sf` probe): the last `fixed-address <ip>;` plus the
// `option subnet-mask <mask>;` in the same/last lease block. nullopt when no
// usable offer is present.
auto ParseDhcpOfferCidr(const std::string& leases) -> std::optional<std::string>;

// Derive the REAL Ethernet mode from ground truth. `last_applied` is the mode the
// firmware itself set (source of truth); the rest is live reality. No carrier or
// no bound address ⇒ kDown; an active dhclient lease (or a firmware-applied DHCP)
// ⇒ kDhcp; an address with no lease ⇒ kStatic. This is what makes a statically
// configured link report static, never a defaulted DHCP (R12).
auto DeriveEthernetMode(sst::network::EthernetMode last_applied, bool carrier, bool has_address,
                        bool dhcp_lease_active) -> sst::network::EthernetMode;

}  // namespace sst::adapters::control
