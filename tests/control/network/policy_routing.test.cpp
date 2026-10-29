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
using sst::adapters::control::BuildFirewallRulesetScript;
using sst::adapters::control::BuildStaSubnetRouteArgv;
using sst::adapters::control::BuildStatelessFirewallRulesetScript;
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

// ---- F7: the whole egress firewall as ONE atomic nft transaction ----

// F7: the firewall is emitted as a SINGLE `nft -f` script (add + flush + chain +
// established-accept + drop, in that order) so nft swaps the ruleset in ONE atomic
// transaction — a re-apply never opens a rule-less window where inbound is accepted
// on the uplink. The old design flushed then re-added via SEPARATE nft invocations.
// Metal finding: the JetPack 7.2 kernel (6.8.12-*-tegra) has no nft_ct module, so the
// stateful ruleset's `ct state` expression is rejected with ENOENT and the egress-only
// bound silently did not exist. This conntrack-free ruleset is the fallback.
TEST(PolicyRoutingCommandTest, StatelessFirewallRulesetDropsInboundSynWithoutConntrack) {
    const std::string script = BuildStatelessFirewallRulesetScript("eth0");
    ASSERT_FALSE(script.empty());

    const std::string table = std::string("inet ") + kUplinkNftTable;
    // Same atomic add -> flush -> chain shape as the stateful script (F7).
    const auto add_table = script.find("add table " + table);
    const auto flush_table = script.find("flush table " + table);
    const auto chain = script.find("add chain " + table + " input");
    const auto syn_drop = script.find("iifname \"eth0\" tcp flags syn / fin,syn,rst,ack drop");
    EXPECT_NE(add_table, std::string::npos);
    EXPECT_NE(flush_table, std::string::npos);
    EXPECT_NE(chain, std::string::npos);
    EXPECT_NE(syn_drop, std::string::npos) << script;
    EXPECT_LT(add_table, flush_table);
    EXPECT_LT(flush_table, chain);
    EXPECT_LT(chain, syn_drop);

    // It must NOT reference conntrack — that is the whole point of the fallback.
    EXPECT_EQ(script.find("ct state"), std::string::npos) << script;
    // And it must NOT blanket-drop inbound: a bare `drop` would also kill the SYN+ACK
    // replies to the camera's own outbound connections (no conntrack to exempt them).
    EXPECT_EQ(script.find("iifname \"eth0\" drop"), std::string::npos) << script;
}

TEST(PolicyRoutingCommandTest, StaSubnetRouteMustLoseToEthernet) {
    EXPECT_EQ(BuildStaSubnetRouteArgv("10.10.1.0/24", "wlP1p1s0"),
              (Argv{"ip", "route", "replace", "10.10.1.0/24", "dev", "wlP1p1s0", "metric", "600"}));
    EXPECT_TRUE(BuildStaSubnetRouteArgv("10.10.1.0/24\naccept", "wlP1p1s0").empty());
}

TEST(PolicyRoutingCommandTest, FirewallAlwaysAllowsLanEthernetSshWithoutConntrack) {
    for (const auto& script : {BuildStatelessFirewallRulesetScript("enP8p1s0"),
                               BuildFirewallRulesetScript("enP8p1s0")}) {
        EXPECT_NE(script.find("ip saddr 10.10.1.0/24 tcp dport 22 accept"), std::string::npos)
            << script;
    }
    EXPECT_EQ(BuildStatelessFirewallRulesetScript("enP8p1s0").find("ct state"), std::string::npos);
}

TEST(PolicyRoutingCommandTest, StatelessFirewallRulesetRefusesUnsafeIface) {
    EXPECT_TRUE(BuildStatelessFirewallRulesetScript("eth0; rm -rf /").empty());
    EXPECT_TRUE(BuildStatelessFirewallRulesetScript("").empty());
    EXPECT_TRUE(BuildStatelessFirewallRulesetScript("-eth0").empty());
}

TEST(PolicyRoutingCommandTest, FirewallRulesetScriptIsOneAtomicTransaction) {
    const std::string script = BuildFirewallRulesetScript("eth0");
    ASSERT_FALSE(script.empty());

    const std::string table = std::string("inet ") + kUplinkNftTable;
    const auto add_table = script.find("add table " + table);
    const auto flush_table = script.find("flush table " + table);
    const auto chain = script.find("add chain " + table + " input");
    const auto established = script.find("ct state established,related accept");
    const auto drop = script.find("iifname \"eth0\" drop");

    // Every directive is present in the SINGLE script (one `nft -f` transaction).
    EXPECT_NE(add_table, std::string::npos);
    EXPECT_NE(flush_table, std::string::npos);
    EXPECT_NE(chain, std::string::npos);
    EXPECT_NE(established, std::string::npos);
    EXPECT_NE(drop, std::string::npos);
    // Ordered add-table -> flush -> chain -> allow-established -> drop: the flush and
    // the re-add live in ONE script, so there is no accept window between them.
    EXPECT_LT(add_table, flush_table);
    EXPECT_LT(flush_table, chain);
    EXPECT_LT(chain, established);
    EXPECT_LT(established, drop);
    // The iface rides only inside a quoted `iifname "<iface>"` literal.
    EXPECT_NE(script.find("iifname \"eth0\""), std::string::npos);
}

// F7 fail-closed: an un-whitelisted iface never produces a script (so it can never be
// written to the temp file or reach `nft -f`).
TEST(PolicyRoutingCommandTest, FirewallRulesetScriptRefusesUnsafeIface) {
    EXPECT_TRUE(BuildFirewallRulesetScript("").empty());
    EXPECT_TRUE(BuildFirewallRulesetScript("eth0; nft flush ruleset").empty());
    EXPECT_TRUE(BuildFirewallRulesetScript("eth0\naccept").empty());
    EXPECT_TRUE(BuildFirewallRulesetScript("-eth0").empty());
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

// F2: the boot egress-only firewall (installed at process start, BEFORE boot-apply
// addresses Ethernet, so no root listener is reachable on a wired uplink in the
// pre-first-Set window) is bounded and never throws — in the container there is no
// wired iface / `nft` so it is a logged no-op, on-device it installs the firewall.
TEST(PolicyRoutingTest, ApplyEgressFirewallIsBoundedAndDoesNotThrow) {
    EXPECT_NO_THROW({ sst::adapters::control::PolicyRouting::ApplyEgressFirewall(); });
}

}  // namespace
