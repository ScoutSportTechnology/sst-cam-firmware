// Different-subnet uplink policy routing + egress-only firewall (U7). The real
// apply/teardown fork `ip`/`nft` and need CAP_NET_ADMIN — those are verified
// ON-DEVICE (they show as bounded no-throw here). The correctness surface tested
// here is the PURE, injection-safe logic: interface-name whitelisting, subnet
// derivation, and the exact `ip rule`/`ip route`/`nft` argv shape — including the
// egress-only firewall that is the security bound for running as root (no camera
// service is reachable on the uplink interface).

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "adapters/control/network/policy-routing.hpp"

namespace {

using sst::adapters::control::BuildFirewallAddChainArgv;
using sst::adapters::control::BuildFirewallAddTableArgv;
using sst::adapters::control::BuildFirewallAllowEstablishedArgv;
using sst::adapters::control::BuildFirewallDeleteTableArgv;
using sst::adapters::control::BuildFirewallDropInboundArgv;
using sst::adapters::control::BuildFirewallFlushTableArgv;
using sst::adapters::control::BuildUplinkDefaultRouteArgv;
using sst::adapters::control::BuildUplinkRouteFlushArgv;
using sst::adapters::control::BuildUplinkRuleAddArgv;
using sst::adapters::control::BuildUplinkRuleDelArgv;
using sst::adapters::control::BuildUplinkSubnetRouteArgv;
using sst::adapters::control::IsSafeIfaceName;
using sst::adapters::control::kUplinkNftTable;
using sst::adapters::control::kUplinkRouteTable;
using sst::adapters::control::kUplinkRulePriority;
using sst::adapters::control::NetworkCidrOf;
using Argv = std::vector<std::string>;

auto Contains(const Argv& argv, const std::string& token) -> bool {
    return std::find(argv.begin(), argv.end(), token) != argv.end();
}

// ---- interface-name whitelisting (injection safety) ----

TEST(PolicyRoutingCommandTest, IfaceNameWhitelist) {
    EXPECT_TRUE(IsSafeIfaceName("eth0"));
    EXPECT_TRUE(IsSafeIfaceName("enP8p1s0"));
    EXPECT_TRUE(IsSafeIfaceName("wlP1p1s0"));
    EXPECT_FALSE(IsSafeIfaceName(""));
    EXPECT_FALSE(IsSafeIfaceName("-eth0"));                 // leading dash (option smuggling)
    EXPECT_FALSE(IsSafeIfaceName("eth0; rm -rf /"));        // shell metacharacters
    EXPECT_FALSE(IsSafeIfaceName("eth0 drop"));             // embedded space
    EXPECT_FALSE(IsSafeIfaceName("eth0\naccept"));          // embedded newline
    EXPECT_FALSE(IsSafeIfaceName("0123456789abcdef1234"));  // > IFNAMSIZ-1
}

// ---- subnet derivation ----

TEST(PolicyRoutingCommandTest, NetworkCidrOf) {
    EXPECT_EQ(NetworkCidrOf("192.168.9.30/24"), "192.168.9.0/24");
    EXPECT_EQ(NetworkCidrOf("10.10.1.142/16"), "10.10.0.0/16");
    EXPECT_EQ(NetworkCidrOf("172.16.5.9/32"), "172.16.5.9/32");
    EXPECT_EQ(NetworkCidrOf("1.2.3.4/0"), "0.0.0.0/0");
    EXPECT_EQ(NetworkCidrOf("192.168.9.30"), "");        // no prefix
    EXPECT_EQ(NetworkCidrOf("192.168.9.30/24\nx"), "");  // content-safety
}

// ---- ip rule / ip route argv (source-based egress selection) ----

TEST(PolicyRoutingCommandTest, RuleAddSelectsUplinkSourceIntoDedicatedTable) {
    const Argv argv = BuildUplinkRuleAddArgv("192.168.9.30");
    EXPECT_EQ(argv, (Argv{"ip", "rule", "add", "from", "192.168.9.30", "table", kUplinkRouteTable,
                          "priority", kUplinkRulePriority}));
    // A firmware-PRIVATE table, distinct from the retired same-subnet table 142.
    EXPECT_STRNE(kUplinkRouteTable, "142");
    EXPECT_STRNE(kUplinkRulePriority, "142");
    // Hostile / malformed source address never reaches an argv.
    EXPECT_TRUE(BuildUplinkRuleAddArgv("192.168.9.30/24").empty());  // CIDR, not a literal
    EXPECT_TRUE(BuildUplinkRuleAddArgv("192.168.9.30; ip rule flush").empty());
    EXPECT_TRUE(BuildUplinkRuleAddArgv("").empty());
}

TEST(PolicyRoutingCommandTest, RuleDelIsByPriority) {
    EXPECT_EQ(BuildUplinkRuleDelArgv(),
              (Argv{"ip", "rule", "del", "priority", kUplinkRulePriority}));
}

TEST(PolicyRoutingCommandTest, SubnetAndDefaultRouteGoIntoTheDedicatedTable) {
    EXPECT_EQ(BuildUplinkSubnetRouteArgv("192.168.9.0/24", "eth0"),
              (Argv{"ip", "route", "replace", "192.168.9.0/24", "dev", "eth0", "table",
                    kUplinkRouteTable}));
    EXPECT_EQ(BuildUplinkDefaultRouteArgv("192.168.9.1", "eth0"),
              (Argv{"ip", "route", "replace", "default", "via", "192.168.9.1", "dev", "eth0",
                    "table", kUplinkRouteTable}));
    EXPECT_EQ(BuildUplinkRouteFlushArgv(),
              (Argv{"ip", "route", "flush", "table", kUplinkRouteTable}));
    // Guarded input.
    EXPECT_TRUE(BuildUplinkSubnetRouteArgv("not-a-cidr", "eth0").empty());
    EXPECT_TRUE(BuildUplinkSubnetRouteArgv("192.168.9.0/24", "eth0; x").empty());
    EXPECT_TRUE(BuildUplinkDefaultRouteArgv("bad", "eth0").empty());
    EXPECT_TRUE(BuildUplinkDefaultRouteArgv("192.168.9.1", "").empty());
}

// ---- egress-only firewall (THE security bound: no inbound service exposure) ----

TEST(PolicyRoutingCommandTest, FirewallTableAndChainScopeToInputHook) {
    EXPECT_EQ(BuildFirewallAddTableArgv(), (Argv{"nft", "add", "table", "inet", kUplinkNftTable}));
    EXPECT_EQ(BuildFirewallFlushTableArgv(),
              (Argv{"nft", "flush", "table", "inet", kUplinkNftTable}));
    EXPECT_EQ(BuildFirewallDeleteTableArgv(),
              (Argv{"nft", "delete", "table", "inet", kUplinkNftTable}));

    const Argv chain = BuildFirewallAddChainArgv();
    EXPECT_TRUE(Contains(chain, "hook"));
    EXPECT_TRUE(Contains(chain, "input"));
    // A default-ACCEPT policy + an iface-scoped match => LAN traffic is untouched.
    EXPECT_TRUE(Contains(chain, "accept"));
}

TEST(PolicyRoutingCommandTest, FirewallDropsAllInboundOnUplinkExceptEstablished) {
    // Established/related return traffic (for the camera's OUTBOUND streaming) is
    // accepted; EVERYTHING else inbound on the uplink is dropped — so NO camera
    // service (RTSP/control/downloads, any port) is reachable on the uplink iface.
    const Argv allow = BuildFirewallAllowEstablishedArgv("eth0");
    EXPECT_EQ(allow, (Argv{"nft", "add", "rule", "inet", kUplinkNftTable, "input", "iifname",
                           "eth0", "ct", "state", "established,related", "accept"}));

    const Argv drop = BuildFirewallDropInboundArgv("eth0");
    EXPECT_EQ(drop, (Argv{"nft", "add", "rule", "inet", kUplinkNftTable, "input", "iifname", "eth0",
                          "drop"}));
    // The drop rule is scoped to the uplink iface (iifname), so it is egress-only —
    // it never drops LAN-inbound traffic on the STA interface.
    EXPECT_TRUE(Contains(drop, "iifname"));
    EXPECT_TRUE(Contains(drop, "drop"));

    // Hostile iface never reaches an nft argv.
    EXPECT_TRUE(BuildFirewallAllowEstablishedArgv("eth0; nft flush ruleset").empty());
    EXPECT_TRUE(BuildFirewallDropInboundArgv("").empty());
}

// ---- adapter live surface (ON-DEVICE): bounded + never throws in the container ----

TEST(PolicyRoutingTest, ApplyAndTeardownAreBoundedAndDoNotThrow) {
    sst::adapters::control::PolicyRouting routing;
    EXPECT_NO_THROW({
        // In the container there is no wired uplink and `ip`/`nft` are absent/fail;
        // the adapter must return without throwing (real steering is on-device).
        (void)routing.ApplyUplinkRouting(
            {.uplink_address_cidr = "192.168.9.30/24", .uplink_gateway = "192.168.9.1"});
        routing.TeardownUplinkRouting();
    });
}

}  // namespace
