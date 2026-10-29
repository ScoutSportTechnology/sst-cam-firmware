// Firmware-owned Ethernet configurator (U3). The real apply/probe fork
// `ip`/`dhclient` and need CAP_NET_ADMIN + a live wired interface — those are
// verified ON-DEVICE (they show as bounded no-throw here). The bulk of the
// correctness surface is the PURE logic these tests exercise directly: strict
// IPv4 validation (the injection/content-safety gate for BLE-supplied
// address/gateway/DNS), argv construction, resolv.conf formatting, DHCP-offer
// lease parsing, and the static-vs-DHCP mode derivation that fixes the
// DHCP-when-static reporting bug (R12/AE5).

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "adapters/control/network/ip-ethernet-command.hpp"
#include "adapters/control/network/ip-ethernet-configurator.hpp"
#include "adapters/control/network/nmcli-uplink-configurator.hpp"
#include "domain/network/models/ethernet-mode.hpp"

namespace {

using sst::adapters::control::BuildEthAddrDelArgv;
using sst::adapters::control::BuildEthAddrReplaceArgv;
using sst::adapters::control::BuildEthRouteReplaceArgv;
using sst::adapters::control::BuildResolvConf;
using sst::adapters::control::BuildStaDhclientArgv;
using sst::adapters::control::DeriveEthernetMode;
using sst::adapters::control::IsIpv4Cidr;
using sst::adapters::control::IsIpv4Literal;
using sst::adapters::control::MaskToPrefix;
using sst::adapters::control::ParseDhcpOfferCidr;
using sst::network::EthernetMode;

TEST(IpEthernetCommandTest, StaDhcpUsesMetricScriptForEveryLease) {
    EXPECT_EQ(BuildStaDhclientArgv("wlP1p1s0"),
              (std::vector<std::string>{"dhclient", "-1", "-4", "-sf",
                                        "/opt/sst-cam/bin/sst-sta-dhclient-script", "wlP1p1s0"}));
    EXPECT_TRUE(BuildStaDhclientArgv("wlP1p1s0; nft flush ruleset").empty());
}

// ---- IPv4 validation (injection / content safety) ----

TEST(IpEthernetCommandTest, AcceptsWellFormedIpv4Literals) {
    EXPECT_TRUE(IsIpv4Literal("10.10.1.30"));
    EXPECT_TRUE(IsIpv4Literal("0.0.0.0"));
    EXPECT_TRUE(IsIpv4Literal("255.255.255.255"));
    EXPECT_TRUE(IsIpv4Literal("192.168.1.1"));
}

TEST(IpEthernetCommandTest, RejectsMalformedAndHostileLiterals) {
    EXPECT_FALSE(IsIpv4Literal(""));
    EXPECT_FALSE(IsIpv4Literal("1.2.3"));      // too few octets
    EXPECT_FALSE(IsIpv4Literal("1.2.3.4.5"));  // too many
    EXPECT_FALSE(IsIpv4Literal("256.1.1.1"));  // octet > 255
    EXPECT_FALSE(IsIpv4Literal("01.2.3.4"));   // leading zero
    EXPECT_FALSE(IsIpv4Literal("1.2.3.4 "));   // trailing space
    EXPECT_FALSE(IsIpv4Literal(" 1.2.3.4"));   // leading space
    EXPECT_FALSE(IsIpv4Literal("1.2.3.-4"));   // sign
    EXPECT_FALSE(IsIpv4Literal("1.2.3.x"));    // non-numeric
    // The content-safety cases: a smuggled newline / extra resolv.conf directive
    // / shell metacharacter must never validate.
    EXPECT_FALSE(IsIpv4Literal("1.2.3.4\nnameserver 6.6.6.6"));
    EXPECT_FALSE(IsIpv4Literal("1.2.3.4; rm -rf /"));
    EXPECT_FALSE(IsIpv4Literal("1.2.3.4\n"));
}

TEST(IpEthernetCommandTest, CidrValidation) {
    EXPECT_TRUE(IsIpv4Cidr("10.10.1.30/24"));
    EXPECT_TRUE(IsIpv4Cidr("0.0.0.0/0"));
    EXPECT_TRUE(IsIpv4Cidr("192.168.1.1/32"));
    EXPECT_FALSE(IsIpv4Cidr("10.10.1.30"));     // no prefix
    EXPECT_FALSE(IsIpv4Cidr("10.10.1.30/33"));  // prefix > 32
    EXPECT_FALSE(IsIpv4Cidr("10.10.1.30/"));    // empty prefix
    EXPECT_FALSE(IsIpv4Cidr("256.1.1.1/24"));   // bad address
    EXPECT_FALSE(IsIpv4Cidr("10.10.1.30/24\nnameserver 6.6.6.6"));
}

// ---- resolv.conf formatting (content safety) ----

TEST(IpEthernetCommandTest, ResolvConfIsASingleValidatedLine) {
    EXPECT_EQ(BuildResolvConf("1.1.1.1"), "nameserver 1.1.1.1\n");
    EXPECT_EQ(BuildResolvConf(""), "");  // empty DNS → no file write
}

// ---- argv construction (fail-fast on invalid input, never exec a hostile arg) ----

TEST(IpEthernetCommandTest, AddrReplaceArgvGuardsInput) {
    const auto argv = BuildEthAddrReplaceArgv("10.10.1.30/24", "eth0");
    ASSERT_EQ(argv.size(), 6U);
    EXPECT_EQ(argv[0], "ip");
    EXPECT_EQ(argv[2], "replace");
    EXPECT_EQ(argv[3], "10.10.1.30/24");
    EXPECT_EQ(argv[5], "eth0");
    // Rejected before exec: bad CIDR, empty iface.
    EXPECT_TRUE(BuildEthAddrReplaceArgv("10.10.1.30", "eth0").empty());
    EXPECT_TRUE(BuildEthAddrReplaceArgv("10.10.1.30/24", "").empty());
    EXPECT_TRUE(BuildEthAddrReplaceArgv("1.2.3.4\nx/24", "eth0").empty());
}

// Provenance teardown (metal finding): removal must be a targeted `ip addr del` of
// ONE address, never a blanket `ip addr flush` that would also destroy an
// out-of-band/operator address and strand the camera off the LAN.
TEST(IpEthernetCommandTest, AddrDelArgvRemovesExactlyOneAddressAndGuardsInput) {
    const auto argv = BuildEthAddrDelArgv("10.10.1.30/24", "eth0");
    ASSERT_EQ(argv.size(), 6U);
    EXPECT_EQ(argv[0], "ip");
    EXPECT_EQ(argv[1], "addr");
    EXPECT_EQ(argv[2], "del");  // NOT "flush" — scoped removal is the whole point
    EXPECT_EQ(argv[3], "10.10.1.30/24");
    EXPECT_EQ(argv[4], "dev");
    EXPECT_EQ(argv[5], "eth0");
    // The address is always named, so a caller cannot accidentally widen this into a
    // flush by passing something empty/garbage.
    EXPECT_TRUE(BuildEthAddrDelArgv("", "eth0").empty());
    EXPECT_TRUE(BuildEthAddrDelArgv("10.10.1.30", "eth0").empty());
    EXPECT_TRUE(BuildEthAddrDelArgv("10.10.1.30/24", "").empty());
    EXPECT_TRUE(BuildEthAddrDelArgv("1.2.3.4\nx/24", "eth0").empty());
}

TEST(IpEthernetCommandTest, RouteReplaceArgvGuardsInput) {
    const auto argv = BuildEthRouteReplaceArgv("10.10.1.1", "eth0");
    ASSERT_EQ(argv.size(), 10U);
    EXPECT_EQ(argv[3], "default");
    EXPECT_EQ(argv[5], "10.10.1.1");
    EXPECT_EQ(argv[7], "eth0");
    EXPECT_TRUE(BuildEthRouteReplaceArgv("not-an-ip", "eth0").empty());
    EXPECT_TRUE(BuildEthRouteReplaceArgv("10.10.1.1", "").empty());
    EXPECT_EQ(BuildEthRouteReplaceArgv("10.10.1.1", "eth0").back(), "100");
}

// ---- netmask → prefix + DHCP-offer parsing (ProbeDhcpOffer mechanic) ----

TEST(IpEthernetCommandTest, MaskToPrefix) {
    EXPECT_EQ(MaskToPrefix("255.255.255.0"), 24);
    EXPECT_EQ(MaskToPrefix("255.255.0.0"), 16);
    EXPECT_EQ(MaskToPrefix("255.255.255.255"), 32);
    EXPECT_EQ(MaskToPrefix("0.0.0.0"), 0);
    EXPECT_EQ(MaskToPrefix("255.255.255.128"), 25);
    EXPECT_FALSE(MaskToPrefix("255.0.255.0").has_value());  // non-contiguous
    EXPECT_FALSE(MaskToPrefix("bogus").has_value());
}

TEST(IpEthernetCommandTest, ParsesOfferedCidrFromLeaseFile) {
    // The body a no-op `-sf` dhclient probe records without binding the address.
    const std::string leases = R"(
lease {
  interface "eth0";
  fixed-address 10.10.1.55;
  option subnet-mask 255.255.255.0;
  option routers 10.10.1.1;
}
)";
    EXPECT_EQ(ParseDhcpOfferCidr(leases), std::optional<std::string>("10.10.1.55/24"));
}

TEST(IpEthernetCommandTest, ParseOfferTakesTheLastLeaseBlock) {
    const std::string leases = R"(
lease { fixed-address 10.0.0.9; option subnet-mask 255.255.0.0; }
lease { fixed-address 192.168.4.7; option subnet-mask 255.255.255.0; }
)";
    EXPECT_EQ(ParseDhcpOfferCidr(leases), std::optional<std::string>("192.168.4.7/24"));
}

TEST(IpEthernetCommandTest, ParseOfferReturnsNulloptWhenIncomplete) {
    EXPECT_FALSE(ParseDhcpOfferCidr("").has_value());
    EXPECT_FALSE(ParseDhcpOfferCidr("lease { option subnet-mask 255.255.255.0; }").has_value());
    // fixed-address but no subnet-mask → cannot form a CIDR.
    EXPECT_FALSE(ParseDhcpOfferCidr("lease { fixed-address 10.0.0.9; }").has_value());
}

// ---- static-vs-DHCP mode derivation (the DHCP-when-static fix, R12/AE5) ----

TEST(IpEthernetCommandTest, StaticApplyReportsStaticNotDhcp) {
    // Firmware applied static; address bound, carrier up, NO dhclient lease → the
    // link reports Static (never a defaulted DHCP — this is the bug fix).
    EXPECT_EQ(DeriveEthernetMode(EthernetMode::kStatic, /*carrier=*/true, /*has_address=*/true,
                                 /*dhcp_lease_active=*/false),
              EthernetMode::kStatic);
}

TEST(IpEthernetCommandTest, DhcpApplyReportsDhcp) {
    EXPECT_EQ(DeriveEthernetMode(EthernetMode::kDhcp, true, true, false), EthernetMode::kDhcp);
    // A live lease reports DHCP even on a fresh instance that never applied.
    EXPECT_EQ(DeriveEthernetMode(EthernetMode::kDown, true, true, /*dhcp_lease_active=*/true),
              EthernetMode::kDhcp);
}

TEST(IpEthernetCommandTest, NoCarrierOrNoAddressIsDown) {
    EXPECT_EQ(DeriveEthernetMode(EthernetMode::kStatic, /*carrier=*/false, true, false),
              EthernetMode::kDown);
    EXPECT_EQ(DeriveEthernetMode(EthernetMode::kDhcp, true, /*has_address=*/false, false),
              EthernetMode::kDown);
}

TEST(IpEthernetCommandTest, UnknownWithAddressAndNoLeaseIsStatic) {
    // Fresh adapter (no last-applied mode), an address present with no lease: the
    // truthful read is "statically assigned".
    EXPECT_EQ(DeriveEthernetMode(EthernetMode::kDown, true, true, false), EthernetMode::kStatic);
}

// ---- adapter live surface (ON-DEVICE): bounded + never throws in the container ----

TEST(IpEthernetConfiguratorTest, ProbeEthernetIsBoundedAndDoesNotThrow) {
    sst::adapters::control::IpEthernetConfigurator cfg;
    // In the cross-compile container there is no wired uplink (or it reads down);
    // the probe must return without throwing — the real static/DHCP readback is
    // verified on-device.
    EXPECT_NO_THROW({
        const auto probe = cfg.ProbeEthernet();
        (void)probe;
    });
}

}  // namespace
