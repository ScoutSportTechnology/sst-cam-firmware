#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/control/ports/wifi-manager.hpp"
#include "app/network/ports/connectivity-arbiter.hpp"
#include "app/network/ports/ethernet-carrier-monitor.hpp"
#include "app/network/ports/ethernet-controller.hpp"
#include "app/network/ports/policy-routing.hpp"
#include "app/network/ports/self-revert-timer.hpp"
#include "app/network/ports/uplink-configurator.hpp"
#include "app/network/services/connectivity_arbiter/connectivity-arbiter.hpp"
#include "domain/config/models/uplink-config.hpp"
#include "domain/network/models/connectivity-mode.hpp"
#include "domain/network/models/ethernet-mode.hpp"

namespace {

using sst::network::ConnectivityMode;
using sst::network::LanPath;

// One shared ordered log across all fakes so tests can assert INV2's ordering:
// the GO teardown (wifi.Stop) must precede the STA join (uplink.ApplyWifiSta),
// and no path opens a concurrent-radio window.
struct CallLog {
    std::vector<std::string> calls;
    auto Record(std::string what) -> void { calls.push_back(std::move(what)); }
};

class FakeWifi final : public sst::control::IWifiManager {
   public:
    explicit FakeWifi(CallLog& log) : log_(log) {}
    auto StartP2pGroupOwner() -> std::optional<sst::network::WifiDirectGroup> override {
        log_.Record("wifi.StartP2pGroupOwner");
        return sst::network::WifiDirectGroup{.ssid = "DIRECT-xy",
                                             .psk = "pw",
                                             .group_interface = "p2p0",
                                             .group_owner_ip = "192.168.49.1",
                                             .role = "GO"};
    }
    auto Stop() -> void override { log_.Record("wifi.Stop"); }
    [[nodiscard]] auto State() const -> sst::control::WifiState override { return {}; }

   private:
    CallLog& log_;
};

class FakeUplink final : public sst::network::IUplinkConfigurator {
   public:
    explicit FakeUplink(CallLog& log) : log_(log) {}
    auto ApplyWifiSta(const sst::config::WifiStaUplink& /*cfg*/)
        -> sst::network::UplinkResult override {
        log_.Record("uplink.ApplyWifiSta");
        ++wifi_calls;
        return sta_result;
    }
    auto TeardownWifiSta() -> void override {
        log_.Record("uplink.TeardownWifiSta");
        ++teardown_calls;
    }
    auto ProbeWifiSta() -> sst::network::UplinkResult override {
        ++probe_calls;
        return probe_result;
    }

    sst::network::UplinkResult sta_result{.ok = true, .detail = "10.0.0.5"};
    // Live STA carrier the D2a watchdog polls; default present.
    sst::network::UplinkResult probe_result{.ok = true, .detail = "10.0.0.5"};
    int wifi_calls{0};
    int teardown_calls{0};
    int probe_calls{0};

   private:
    CallLog& log_;
};

class FakeTimer final : public sst::network::ISelfRevertTimer {
   public:
    explicit FakeTimer(CallLog& log) : log_(log) {}
    auto Arm(std::chrono::milliseconds /*after*/,
             std::function<void()> on_expire) -> void override {
        log_.Record("timer.Arm");
        armed_ = true;
        on_expire_ = std::move(on_expire);
    }
    auto Disarm() -> void override {
        log_.Record("timer.Disarm");
        armed_ = false;
    }
    // Simulate expiry from the timer thread (the arbiter holds no lock here).
    auto Fire() -> void {
        if (armed_ && on_expire_) {
            on_expire_();
        }
    }
    [[nodiscard]] auto armed() const -> bool { return armed_; }

   private:
    CallLog& log_;
    bool armed_{false};
    std::function<void()> on_expire_;
};

class FakeHooks final : public sst::network::IConnectivityTransitionHooks {
   public:
    explicit FakeHooks(CallLog& log) : log_(log) {}
    auto OnSharedWifiJoined(const std::string& sta_lan_ip) -> std::string override {
        log_.Record("hooks.OnSharedWifiJoined");
        last_sta_ip = sta_lan_ip;
        ++joined_calls;
        return credential;
    }
    auto OnRevertedToP2p() -> bool override {
        log_.Record("hooks.OnRevertedToP2p");
        ++reverted_calls;
        return revert_ok;
    }
    auto OnEthernetLanSelected(const std::string& ethernet_lan_ip) -> std::string override {
        log_.Record("hooks.OnEthernetLanSelected");
        last_ethernet_ip = ethernet_lan_ip;
        ++ethernet_selected_calls;
        return ethernet_credential;
    }

    std::string credential{"cam:secret"};
    std::string ethernet_credential;  // empty by default (U4: preview rebind is U6)
    std::string last_ethernet_ip;
    int ethernet_selected_calls{0};
    std::string last_sta_ip;
    int joined_calls{0};
    int reverted_calls{0};
    bool revert_ok{true};

   private:
    CallLog& log_;
};

// Firmware-owned Ethernet fake: scriptable probe (mode/address/carrier), apply
// outcome, and DHCP offer so the LAN-path decision matrix is deterministic.
class FakeEthernet final : public sst::network::IEthernetController {
   public:
    explicit FakeEthernet(CallLog& log) : log_(log) {}
    auto ApplyEthernet(const sst::config::EthernetUplink& /*cfg*/)
        -> sst::network::EthernetApplyResult override {
        log_.Record("ethernet.Apply");
        ++apply_calls;
        return apply_ok ? sst::network::EthernetApplyResult{.ok = true, .detail = apply_address}
                        : sst::network::EthernetApplyResult{.ok = false, .detail = "apply failed"};
    }
    auto ProbeEthernet() -> sst::network::EthernetProbe override {
        // F3: a test hook fired on EVERY probe. ProbeEthernet is only ever called
        // OFF the arbiter lock, so a hook that re-enters a BLE-facing getter
        // (CurrentLanPath) proves the lock was dropped across the shell-out — a
        // regressed F3 (probe under `mtx_`) would self-deadlock here.
        if (on_probe) {
            on_probe();
        }
        return {
            .mode = probe_mode, .address = probe_address, .carrier = carrier, .gateway = gateway};
    }
    auto TeardownEthernet() -> void override {
        log_.Record("ethernet.Teardown");
        ++teardown_calls;
    }
    auto ProbeDhcpOffer() -> std::optional<std::string> override {
        ++offer_calls;
        return dhcp_offer;
    }

    // Scriptable knobs.
    bool carrier{false};
    sst::network::EthernetMode probe_mode{sst::network::EthernetMode::kStatic};
    std::string probe_address;
    std::string gateway;  // default gateway the probe reports (U7 uplink routing)
    bool apply_ok{true};
    std::string apply_address{"10.10.1.30/24"};
    std::optional<std::string> dhcp_offer{std::nullopt};
    std::function<void()> on_probe;  // F3: fired inside every (off-lock) ProbeEthernet
    int apply_calls{0};
    int teardown_calls{0};
    int offer_calls{0};

   private:
    CallLog& log_;
};

// Ethernet-carrier monitor fake: captures the arbiter callback so a test drives
// carrier transitions synchronously (like FakeTimer::Fire).
class FakeEthernetMonitor final : public sst::network::IEthernetCarrierMonitor {
   public:
    auto Start(std::function<void()> on_poll) -> void override { on_poll_ = std::move(on_poll); }
    auto Stop() -> void override { on_poll_ = nullptr; }
    // Simulate one carrier poll from the monitor thread.
    auto Poll() -> void {
        if (on_poll_) {
            on_poll_();
        }
    }

   private:
    std::function<void()> on_poll_;
};

// Policy-routing fake (U7): records apply/teardown for the different-subnet uplink
// so the R4 apply/teardown transitions are asserted without touching `ip`/`nft`.
class FakePolicyRouting final : public sst::network::IPolicyRouting {
   public:
    auto ApplyUplinkRouting(const sst::network::UplinkRoutingConfig& cfg) -> bool override {
        ++apply_calls;
        last_cfg = cfg;
        return apply_ok;
    }
    auto TeardownUplinkRouting() -> void override { ++teardown_calls; }

    bool apply_ok{true};
    int apply_calls{0};
    int teardown_calls{0};
    sst::network::UplinkRoutingConfig last_cfg;
};

struct Harness {
    CallLog log;
    FakeWifi wifi{log};
    FakeUplink uplink{log};
    FakeTimer timer{log};
    FakeHooks hooks{log};
    FakeEthernet ethernet{log};
    FakeEthernetMonitor monitor;
    FakePolicyRouting policy_routing;
    sst::network::ConnectivityArbiter arbiter{
        wifi,     uplink,  timer,          hooks,
        ethernet, monitor, policy_routing, std::chrono::milliseconds{5000}};
};

// Enable-wifi uplink config helper (ethernet disabled).
auto WifiConfig(const std::string& ssid) -> sst::config::UplinkData {
    sst::config::UplinkData cfg;
    cfg.wifi.enabled = true;
    cfg.wifi.ssid = ssid;
    return cfg;
}

// Static-ethernet uplink config helper (wifi disabled).
auto StaticEthernetConfig(const std::string& cidr,
                          bool wifi_enabled = false) -> sst::config::UplinkData {
    sst::config::UplinkData cfg;
    cfg.ethernet.enabled = true;
    cfg.ethernet.dhcp = false;
    cfg.ethernet.address = cidr;
    cfg.wifi.enabled = wifi_enabled;
    cfg.wifi.ssid = wifi_enabled ? std::optional<std::string>{"PhoneNet"} : std::nullopt;
    return cfg;
}

auto IndexOf(const std::vector<std::string>& calls, const std::string& what) -> int {
    for (int i = 0; i < static_cast<int>(calls.size()); ++i) {
        if (calls[i] == what) {
            return i;
        }
    }
    return -1;
}

// R-BOOT / AE8: constructed arbiter is kP2pOnly and applies no stored config.
TEST(ConnectivityArbiterTest, BootStartsInP2pOnlyAndAppliesNothing) {
    Harness harness;
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
    EXPECT_TRUE(harness.log.calls.empty());
    EXPECT_EQ(harness.uplink.wifi_calls, 0);
}

// Happy + ordering (AE2/AE5): GO stopped BEFORE STA applied, lands kMigrating.
TEST(ConnectivityArbiterTest, RequestSharedWifiStopsGoBeforeApplyingSta) {
    Harness harness;
    sst::config::WifiStaUplink cfg;
    cfg.ssid = "PhoneNet";

    const auto migration = harness.arbiter.RequestSharedWifi(cfg);

    if (!migration.has_value()) {
        FAIL();
        return;
    }
    EXPECT_EQ(migration.value().id, 1U);
    EXPECT_EQ(migration.value().preview_endpoint, "10.0.0.5");
    EXPECT_EQ(migration.value().preview_credential, "cam:secret");
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kMigrating);

    const int stop_idx = IndexOf(harness.log.calls, "wifi.Stop");
    const int apply_idx = IndexOf(harness.log.calls, "uplink.ApplyWifiSta");
    ASSERT_GE(stop_idx, 0);
    ASSERT_GE(apply_idx, 0);
    EXPECT_LT(stop_idx, apply_idx) << "INV2: GO teardown must precede STA join";
    EXPECT_TRUE(harness.timer.armed());
}

// STA join fails → arbiter reverts to a legal S1, returns nullopt.
TEST(ConnectivityArbiterTest, StaJoinFailureRevertsToP2p) {
    Harness harness;
    harness.uplink.sta_result = {.ok = false, .detail = "wrong passphrase"};
    sst::config::WifiStaUplink cfg;

    const auto migration = harness.arbiter.RequestSharedWifi(cfg);

    EXPECT_FALSE(migration.has_value());
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
    EXPECT_EQ(harness.hooks.reverted_calls, 1);
    EXPECT_EQ(harness.uplink.teardown_calls, 1);  // STA cleanly torn down on revert
    EXPECT_FALSE(harness.timer.armed());
    // Still ordered: Stop happened, ApplyWifiSta happened, then revert.
    EXPECT_LT(IndexOf(harness.log.calls, "wifi.Stop"),
              IndexOf(harness.log.calls, "uplink.ApplyWifiSta"));
}

// Confirm epoch (AE2): in-epoch reachable confirm → kStaOnly, timer disarmed.
TEST(ConnectivityArbiterTest, InEpochConfirmFinalisesStaOnly) {
    Harness harness;
    sst::config::WifiStaUplink cfg;
    const auto migration = harness.arbiter.RequestSharedWifi(cfg);
    if (!migration.has_value()) {
        FAIL();
        return;
    }

    const auto mode = harness.arbiter.ConfirmSharedWifi(migration.value().id, true);

    EXPECT_EQ(mode, ConnectivityMode::kStaOnly);
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kStaOnly);
    // The migration self-revert is disarmed, but the SAME timer is now re-armed as
    // the S2 carrier watchdog (D2a) — so it is armed, not idle.
    EXPECT_TRUE(harness.timer.armed());
    EXPECT_EQ(harness.hooks.reverted_calls, 0);
}

// Stale confirm id → ignored, no transition.
TEST(ConnectivityArbiterTest, StaleConfirmIsIgnored) {
    Harness harness;
    sst::config::WifiStaUplink cfg;
    const auto migration = harness.arbiter.RequestSharedWifi(cfg);
    if (!migration.has_value()) {
        FAIL();
        return;
    }

    const auto mode = harness.arbiter.ConfirmSharedWifi(migration.value().id + 99, true);

    EXPECT_EQ(mode, ConnectivityMode::kMigrating);  // unchanged
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kMigrating);
    EXPECT_TRUE(harness.timer.armed());
}

// Deny (isolation, AE3/AE5): in-epoch not-reachable → revert to kP2pOnly.
TEST(ConnectivityArbiterTest, DenyRevertsToP2p) {
    Harness harness;
    sst::config::WifiStaUplink cfg;
    const auto migration = harness.arbiter.RequestSharedWifi(cfg);
    if (!migration.has_value()) {
        FAIL();
        return;
    }

    const auto mode = harness.arbiter.ConfirmSharedWifi(migration.value().id, false);

    EXPECT_EQ(mode, ConnectivityMode::kP2pOnly);
    EXPECT_EQ(harness.hooks.reverted_calls, 1);
    EXPECT_EQ(harness.uplink.teardown_calls, 1);  // STA cleanly torn down on revert
    EXPECT_FALSE(harness.timer.armed());
}

// Re-entrancy (AE5): a second request while kMigrating is rejected — the radio is
// never driven a second time concurrently.
TEST(ConnectivityArbiterTest, ReentrantRequestWhileMigratingIsRejected) {
    Harness harness;
    sst::config::WifiStaUplink cfg;
    ASSERT_TRUE(harness.arbiter.RequestSharedWifi(cfg).has_value());

    const auto second = harness.arbiter.RequestSharedWifi(cfg);

    EXPECT_FALSE(second.has_value());
    EXPECT_EQ(harness.uplink.wifi_calls, 1);  // STA applied exactly once, not twice
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kMigrating);
}

// A session finalisation (OnWifiStopped) modelled as RevertToP2p mid-migration →
// ends in a legal state.
TEST(ConnectivityArbiterTest, RevertMidMigrationEndsLegal) {
    Harness harness;
    sst::config::WifiStaUplink cfg;
    ASSERT_TRUE(harness.arbiter.RequestSharedWifi(cfg).has_value());

    harness.arbiter.RevertToP2p();

    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
    EXPECT_EQ(harness.hooks.reverted_calls, 1);
    EXPECT_EQ(harness.uplink.teardown_calls, 1);  // STA cleanly torn down on revert
    EXPECT_FALSE(harness.timer.armed());
}

// Self-revert timeout with no confirm → kP2pOnly.
TEST(ConnectivityArbiterTest, SelfRevertTimeoutRevertsToP2p) {
    Harness harness;
    sst::config::WifiStaUplink cfg;
    ASSERT_TRUE(harness.arbiter.RequestSharedWifi(cfg).has_value());

    harness.timer.Fire();  // simulate the backstop expiring with no app confirm

    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
    EXPECT_EQ(harness.hooks.reverted_calls, 1);
    EXPECT_EQ(harness.uplink.teardown_calls, 1);  // STA cleanly torn down on revert
}

// A late confirm after the self-revert already fired (stale id) → no re-entry.
TEST(ConnectivityArbiterTest, LateConfirmAfterSelfRevertDoesNotReenterSta) {
    Harness harness;
    sst::config::WifiStaUplink cfg;
    const auto migration = harness.arbiter.RequestSharedWifi(cfg);
    if (!migration.has_value()) {
        FAIL();
        return;
    }

    harness.timer.Fire();  // self-reverts to kP2pOnly, migration id retired
    const auto mode = harness.arbiter.ConfirmSharedWifi(migration.value().id, true);

    EXPECT_EQ(mode, ConnectivityMode::kP2pOnly);
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
}

// After finalising kStaOnly, a fresh migration mints a NEW id (monotonic epochs).
TEST(ConnectivityArbiterTest, MigrationIdsAreMonotonic) {
    Harness harness;
    sst::config::WifiStaUplink cfg;
    const auto first = harness.arbiter.RequestSharedWifi(cfg);
    if (!first.has_value()) {
        FAIL();
        return;
    }
    harness.arbiter.ConfirmSharedWifi(first.value().id, false);  // back to P2P

    const auto second = harness.arbiter.RequestSharedWifi(cfg);
    if (!second.has_value()) {
        FAIL();
        return;
    }
    EXPECT_GT(second.value().id, first.value().id);
}

// U3: the arbiter tracks the SSID it is joining / connected to, and clears it on
// revert (so CurrentSsid is only meaningful while kStaOnly).
TEST(ConnectivityArbiterTest, CurrentSsidTracksTheConnectedNetwork) {
    Harness harness;
    sst::config::WifiStaUplink cfg;
    cfg.ssid = "NetA";

    EXPECT_EQ(harness.arbiter.CurrentSsid(), "");  // nothing joined yet
    const auto migration = harness.arbiter.RequestSharedWifi(cfg);
    if (!migration.has_value()) {
        FAIL();
        return;
    }
    EXPECT_EQ(harness.arbiter.CurrentSsid(), "NetA");  // tracked from the request
    harness.arbiter.ConfirmSharedWifi(migration.value().id, true);
    EXPECT_EQ(harness.arbiter.CurrentSsid(), "NetA");  // still tracked in kStaOnly

    harness.arbiter.RevertToP2p();
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
    EXPECT_EQ(harness.arbiter.CurrentSsid(), "");  // cleared on revert
}

// U3 / R3: a request while kStaOnly for a DIFFERENT SSID is a change-networks —
// the arbiter reverts (tears the old STA down first) then migrates afresh with a
// new epoch, ending kMigrating on the new SSID.
TEST(ConnectivityArbiterTest, ChangeNetworksFromStaOnlyRevertsThenMigrates) {
    Harness harness;
    sst::config::WifiStaUplink cfg_a;
    cfg_a.ssid = "NetA";
    const auto first = harness.arbiter.RequestSharedWifi(cfg_a);
    if (!first.has_value()) {
        FAIL();
        return;
    }
    ASSERT_EQ(harness.arbiter.ConfirmSharedWifi(first.value().id, true),
              ConnectivityMode::kStaOnly);

    sst::config::WifiStaUplink cfg_b;
    cfg_b.ssid = "NetB";
    const auto second = harness.arbiter.RequestSharedWifi(cfg_b);

    if (!second.has_value()) {
        FAIL();
        return;
    }
    EXPECT_GT(second.value().id, first.value().id);  // fresh epoch minted
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kMigrating);
    EXPECT_EQ(harness.arbiter.CurrentSsid(), "NetB");
    EXPECT_EQ(harness.uplink.teardown_calls, 1);  // old STA torn down before the new join
    EXPECT_EQ(harness.uplink.wifi_calls, 2);      // STA applied for NetA, then NetB
    EXPECT_EQ(harness.hooks.reverted_calls, 1);   // GO restored during the internal revert
    EXPECT_TRUE(harness.timer.armed());           // self-revert re-armed for the new epoch
}

// U3: after a change-networks, only the NEW migration id finalises; a stale
// confirm for the superseded old id is ignored (epoch guard holds across the
// internal revert).
TEST(ConnectivityArbiterTest, ChangeNetworksConfirmHonoursOnlyTheNewEpoch) {
    Harness harness;
    sst::config::WifiStaUplink cfg_a;
    cfg_a.ssid = "NetA";
    const auto first = harness.arbiter.RequestSharedWifi(cfg_a);
    if (!first.has_value()) {
        FAIL();
        return;
    }
    harness.arbiter.ConfirmSharedWifi(first.value().id, true);

    sst::config::WifiStaUplink cfg_b;
    cfg_b.ssid = "NetB";
    const auto second = harness.arbiter.RequestSharedWifi(cfg_b);
    if (!second.has_value()) {
        FAIL();
        return;
    }

    // Stale confirm for the retired NetA epoch → no-op, still migrating on NetB.
    EXPECT_EQ(harness.arbiter.ConfirmSharedWifi(first.value().id, true),
              ConnectivityMode::kMigrating);
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kMigrating);

    EXPECT_EQ(harness.arbiter.ConfirmSharedWifi(second.value().id, true),
              ConnectivityMode::kStaOnly);
    EXPECT_EQ(harness.arbiter.CurrentSsid(), "NetB");
}

// U3: a change-networks whose new STA join FAILS reverts to a legal kP2pOnly and
// clears the tracked SSID — it does not linger on the old or the failed network.
TEST(ConnectivityArbiterTest, ChangeNetworksJoinFailureRevertsAndClearsSsid) {
    Harness harness;
    sst::config::WifiStaUplink cfg_a;
    cfg_a.ssid = "NetA";
    const auto first = harness.arbiter.RequestSharedWifi(cfg_a);
    if (!first.has_value()) {
        FAIL();
        return;
    }
    harness.arbiter.ConfirmSharedWifi(first.value().id, true);

    harness.uplink.sta_result = {.ok = false, .detail = "wrong passphrase"};
    sst::config::WifiStaUplink cfg_b;
    cfg_b.ssid = "NetB";
    const auto second = harness.arbiter.RequestSharedWifi(cfg_b);

    EXPECT_FALSE(second.has_value());
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
    EXPECT_EQ(harness.arbiter.CurrentSsid(), "");
    EXPECT_FALSE(harness.timer.armed());
}

// Drive an arbiter to a settled kStaOnly (S2) with the carrier watchdog armed.
auto EnterStaOnly(Harness& harness) -> void {
    sst::config::WifiStaUplink cfg;
    cfg.ssid = "NetA";
    const auto migration = harness.arbiter.RequestSharedWifi(cfg);
    if (!migration.has_value()) {
        FAIL();
        return;
    }
    harness.arbiter.ConfirmSharedWifi(migration.value().id, true);
}

// D2a: while kStaOnly, a SUSTAINED carrier loss (N consecutive failed checks)
// auto-reverts to P2P so preview resumes over the GO even with a dead BLE link.
TEST(ConnectivityArbiterTest, S2CarrierLossWatchdogAutoRevertsAfterStrikes) {
    Harness harness;
    EnterStaOnly(harness);
    ASSERT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kStaOnly);

    harness.uplink.probe_result = {.ok = false, .detail = "not connected"};
    // First two lost-carrier checks keep watching (below the strike threshold).
    harness.timer.Fire();
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kStaOnly);
    harness.timer.Fire();
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kStaOnly);
    // The third consecutive loss trips the watchdog → revert to P2P.
    harness.timer.Fire();
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
    EXPECT_EQ(harness.hooks.reverted_calls, 1);
    EXPECT_EQ(harness.uplink.teardown_calls, 1);
    EXPECT_FALSE(harness.timer.armed());  // watchdog disarmed after the revert

    // A stale tick after the revert is a harmless no-op (no second revert).
    harness.timer.Fire();
    EXPECT_EQ(harness.hooks.reverted_calls, 1);
}

// D2a: a present carrier keeps S2 indefinitely — the watchdog re-arms and never
// reverts while the link is healthy.
TEST(ConnectivityArbiterTest, S2CarrierPresentKeepsStaOnly) {
    Harness harness;
    EnterStaOnly(harness);

    constexpr int kHealthyChecks = 5;
    for (int i = 0; i < kHealthyChecks; ++i) {
        harness.timer.Fire();  // probe_result defaults to ok (carrier present)
    }
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kStaOnly);
    EXPECT_EQ(harness.hooks.reverted_calls, 0);
    EXPECT_TRUE(harness.timer.armed());
}

// D2a: a recovered carrier resets the loss streak — a brief flap does not
// accumulate toward the revert threshold.
TEST(ConnectivityArbiterTest, S2CarrierRecoveryResetsLossStreak) {
    Harness harness;
    EnterStaOnly(harness);

    harness.uplink.probe_result = {.ok = false, .detail = "not connected"};
    harness.timer.Fire();  // streak 1
    harness.timer.Fire();  // streak 2
    harness.uplink.probe_result = {.ok = true, .detail = "10.0.0.5"};
    harness.timer.Fire();  // carrier back → streak reset to 0
    harness.uplink.probe_result = {.ok = false, .detail = "not connected"};
    harness.timer.Fire();  // streak 1 (not 3)
    harness.timer.Fire();  // streak 2

    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kStaOnly);
    EXPECT_EQ(harness.hooks.reverted_calls, 0);
}

// ─────────────────────────── U4: LAN-path selection ───────────────────────────

// Both enabled: static ethernet on `eth_cidr` + wifi `ssid`.
auto BothEnabledConfig(const std::string& eth_cidr,
                       const std::string& ssid) -> sst::config::UplinkData {
    sst::config::UplinkData cfg;
    cfg.ethernet.enabled = true;
    cfg.ethernet.dhcp = false;
    cfg.ethernet.address = eth_cidr;
    cfg.wifi.enabled = true;
    cfg.wifi.ssid = ssid;
    return cfg;
}

// Boot: the LAN path defaults to P2P (the phone reaches the camera over the GO).
TEST(ConnectivityArbiterTest, BootLanPathIsP2p) {
    Harness harness;
    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kP2p);
}

// AE1: Ethernet on the phone's subnet + no STA -> LanPath=kEthernet, ethernet
// applied, NO STA IP brought up on that subnet.
TEST(ConnectivityArbiterTest, EthernetOnPhoneSubnetSelectedNoStaBroughtUp) {
    Harness harness;
    harness.ethernet.carrier = true;

    const auto sel = harness.arbiter.SelectLanPath(
        StaticEthernetConfig("10.10.1.30/24", /*wifi_enabled=*/true), "10.10.1.0/24");

    EXPECT_TRUE(sel.ok);
    EXPECT_EQ(sel.path, LanPath::kEthernet);
    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kEthernet);
    EXPECT_EQ(harness.ethernet.apply_calls, 1);
    EXPECT_EQ(harness.uplink.wifi_calls, 0);      // STA never brought up
    EXPECT_EQ(harness.uplink.teardown_calls, 0);  // and never suppressed
    EXPECT_EQ(sel.endpoint, "10.10.1.30");        // bare Ethernet IP
    EXPECT_NE(sel.confirm_id, 0U);                // handshake pending
    EXPECT_EQ(harness.arbiter.EthernetConfirmId(), sel.confirm_id);
}

// AE2: no Ethernet carrier, STA provisioned -> LanPath=kWifiSta; GO torn down
// before STA (INV2); confirm settles kStaOnly.
TEST(ConnectivityArbiterTest, StaOnlySelectedViaLanPathWhenNoEthernet) {
    Harness harness;
    harness.ethernet.carrier = false;  // no ethernet

    const auto sel = harness.arbiter.SelectLanPath(WifiConfig("PhoneNet"), "10.10.1.0/24");

    EXPECT_TRUE(sel.ok);
    EXPECT_EQ(sel.path, LanPath::kWifiSta);
    EXPECT_LT(IndexOf(harness.log.calls, "wifi.Stop"),
              IndexOf(harness.log.calls, "uplink.ApplyWifiSta"));  // INV2 ordering
    EXPECT_EQ(harness.ethernet.apply_calls, 0);

    EXPECT_EQ(harness.arbiter.ConfirmLanPath(sel.confirm_id, LanPath::kWifiSta, true),
              LanPath::kWifiSta);
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kStaOnly);
    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);
}

// Ethernet on a DIFFERENT subnet is NOT a LAN path: STA remains the LAN and is NOT
// suppressed. F1: the uplink address IS bound (a real ApplyEthernet) so the egress
// policy routing/firewall can be installed against it — the bind is an UPLINK bind,
// not a LAN selection (LanPath stays kWifiSta).
TEST(ConnectivityArbiterTest, EthernetDifferentSubnetNotSelectedStaRemains) {
    Harness harness;
    harness.uplink.sta_result = {.ok = true, .detail = "10.10.1.142"};
    harness.uplink.probe_result = {.ok = true, .detail = "10.10.1.142"};
    // Bring STA up first (ethernet carrier down so STA wins).
    harness.ethernet.carrier = false;
    const auto sta = harness.arbiter.SelectLanPath(BothEnabledConfig("192.168.9.30/24", "PhoneNet"),
                                                   "10.10.1.0/24");
    ASSERT_EQ(sta.path, LanPath::kWifiSta);
    harness.arbiter.ConfirmLanPath(sta.confirm_id, LanPath::kWifiSta, true);
    const int teardowns_before = harness.uplink.teardown_calls;

    // Ethernet appears on a DIFFERENT subnet than the phone's.
    harness.ethernet.carrier = true;
    harness.ethernet.probe_address = "192.168.9.30/24";
    harness.monitor.Poll();

    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);  // STA still the LAN (not eth)
    EXPECT_EQ(harness.ethernet.apply_calls, 1);                      // F1: uplink address bound
    EXPECT_EQ(harness.uplink.teardown_calls, teardowns_before);      // STA not suppressed
    EXPECT_EQ(harness.policy_routing.apply_calls, 1);  // uplink policy routing + firewall applied
}

// U7 / R4 / AE3: a different-subnet Ethernet uplink APPLIES policy routing (the
// uplink address + gateway flow into the routing config) while STA stays the LAN —
// the one legit concurrent dual-interface config.
TEST(ConnectivityArbiterTest, EthernetDifferentSubnetAppliesUplinkPolicyRouting) {
    Harness harness;
    harness.uplink.sta_result = {.ok = true, .detail = "10.10.1.142"};
    harness.uplink.probe_result = {.ok = true, .detail = "10.10.1.142"};
    harness.ethernet.carrier = false;  // STA wins the LAN first
    const auto sta = harness.arbiter.SelectLanPath(BothEnabledConfig("192.168.9.30/24", "PhoneNet"),
                                                   "10.10.1.0/24");
    ASSERT_EQ(sta.path, LanPath::kWifiSta);
    harness.arbiter.ConfirmLanPath(sta.confirm_id, LanPath::kWifiSta, true);

    // Ethernet appears on a different subnet WITH a gateway.
    harness.ethernet.carrier = true;
    harness.ethernet.probe_address = "192.168.9.30/24";
    harness.ethernet.gateway = "192.168.9.1";
    harness.monitor.Poll();

    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);  // STA still the LAN (R4)
    EXPECT_EQ(harness.policy_routing.apply_calls, 1);                // uplink routing applied
    EXPECT_EQ(harness.policy_routing.last_cfg.uplink_address_cidr, "192.168.9.30/24");
    EXPECT_EQ(harness.policy_routing.last_cfg.uplink_gateway, "192.168.9.1");
}

// U7 / R4: when the different-subnet uplink is UNPLUGGED, the policy routing is
// torn down (exactly once) so no stale table/firewall lingers.
TEST(ConnectivityArbiterTest, EthernetUplinkUnplugTearsDownPolicyRouting) {
    Harness harness;
    harness.uplink.sta_result = {.ok = true, .detail = "10.10.1.142"};
    harness.uplink.probe_result = {.ok = true, .detail = "10.10.1.142"};
    harness.ethernet.carrier = false;
    const auto sta = harness.arbiter.SelectLanPath(BothEnabledConfig("192.168.9.30/24", "PhoneNet"),
                                                   "10.10.1.0/24");
    ASSERT_EQ(sta.path, LanPath::kWifiSta);
    harness.arbiter.ConfirmLanPath(sta.confirm_id, LanPath::kWifiSta, true);
    harness.ethernet.carrier = true;
    harness.ethernet.probe_address = "192.168.9.30/24";
    harness.ethernet.gateway = "192.168.9.1";
    harness.monitor.Poll();
    ASSERT_EQ(harness.policy_routing.apply_calls, 1);

    // Cable pulled -> the carrier transition re-arbitrates and tears the uplink down.
    harness.ethernet.carrier = false;
    harness.monitor.Poll();

    EXPECT_EQ(harness.policy_routing.teardown_calls, 1);
    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);  // STA unaffected
}

TEST(ConnectivityArbiterTest, EthernetAddressRenewalReappliesUplinkFirewallWithoutCarrierEdge) {
    Harness harness;
    harness.uplink.sta_result = {.ok = true, .detail = "10.10.1.142"};
    harness.uplink.probe_result = {.ok = true, .detail = "10.10.1.142"};
    const auto sta = harness.arbiter.SelectLanPath(BothEnabledConfig("192.168.9.30/24", "PhoneNet"),
                                                   "10.10.1.0/24");
    ASSERT_EQ(sta.path, LanPath::kWifiSta);
    harness.arbiter.ConfirmLanPath(sta.confirm_id, LanPath::kWifiSta, true);
    harness.ethernet.carrier = true;
    harness.ethernet.probe_address = "192.168.9.30/24";
    harness.monitor.Poll();
    ASSERT_EQ(harness.policy_routing.apply_calls, 1);

    harness.ethernet.probe_address = "192.168.50.7/24";
    harness.ethernet.apply_address = "192.168.50.7/24";
    harness.monitor.Poll();

    EXPECT_EQ(harness.policy_routing.apply_calls, 2);
    EXPECT_EQ(harness.policy_routing.last_cfg.uplink_address_cidr, "192.168.50.7/24");
    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);
}

// AE7 / R19: candidate Ethernet, phone probe FAILS -> de-select, fall to STA; the
// STA fallback is never suppressed prematurely (it is only brought up AFTER the
// Ethernet probe fails).
TEST(ConnectivityArbiterTest, EthernetProbeFailFallsToStaNeverSuppressesPrematurely) {
    Harness harness;
    harness.ethernet.carrier = true;

    const auto sel = harness.arbiter.SelectLanPath(BothEnabledConfig("10.10.1.30/24", "PhoneNet"),
                                                   "10.10.1.0/24");
    ASSERT_EQ(sel.path, LanPath::kEthernet);
    EXPECT_EQ(harness.uplink.wifi_calls, 0);      // STA never brought up while Ethernet pending
    EXPECT_EQ(harness.uplink.teardown_calls, 0);  // and never suppressed

    const auto committed =
        harness.arbiter.ConfirmLanPath(sel.confirm_id, LanPath::kEthernet, false);

    EXPECT_EQ(harness.ethernet.teardown_calls, 1);  // Ethernet de-selected (torn down)
    EXPECT_EQ(committed, LanPath::kWifiSta);        // fell to STA
    EXPECT_EQ(harness.uplink.wifi_calls, 1);        // STA brought up only AFTER the fail
    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);
}

// AE10 / R20: mid-session same-subnet STATIC Ethernet hotplug tears the STA IP
// down BEFORE binding Ethernet (no transient dual-home), and the mode-independent
// monitor is what fires it.
TEST(ConnectivityArbiterTest, SameSubnetStaticHotplugSuppressesStaBeforeEthernet) {
    Harness harness;
    harness.ethernet.carrier = false;  // STA wins first
    const auto sta = harness.arbiter.SelectLanPath(BothEnabledConfig("10.10.1.30/24", "PhoneNet"),
                                                   "10.10.1.0/24");
    ASSERT_EQ(sta.path, LanPath::kWifiSta);
    harness.arbiter.ConfirmLanPath(sta.confirm_id, LanPath::kWifiSta, true);
    ASSERT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kStaOnly);
    harness.log.calls.clear();

    // Cable plugged in on the same subnet.
    harness.ethernet.carrier = true;
    harness.monitor.Poll();

    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kEthernet);
    EXPECT_EQ(harness.ethernet.apply_calls, 1);
    // No transient dual-home: STA torn down BEFORE the Ethernet address is applied.
    EXPECT_LT(IndexOf(harness.log.calls, "uplink.TeardownWifiSta"),
              IndexOf(harness.log.calls, "ethernet.Apply"));
    // Unsolicited hotplug "notification": a fresh Ethernet handshake the app
    // re-probes on its next Get (pull model).
    EXPECT_NE(harness.arbiter.EthernetConfirmId(), 0U);
}

// Ground-truth reconciliation (R2/A4): the phone reports a DIFFERENT subnet, but
// STA's REAL address falls inside Ethernet's REAL subnet -> refuse the dual-config
// and select Ethernet (single-path).
TEST(ConnectivityArbiterTest, GroundTruthReconciliationRefusesBadCidrDualHome) {
    Harness harness;
    harness.uplink.sta_result = {.ok = true, .detail = "10.0.0.5"};
    harness.ethernet.carrier = false;
    // STA up; the phone (mis)reports a different subnet than the real 10.0.0.0/24.
    const auto sta = harness.arbiter.SelectLanPath(WifiConfig("PhoneNet"), "192.168.5.0/24");
    ASSERT_EQ(sta.path, LanPath::kWifiSta);
    harness.arbiter.ConfirmLanPath(sta.confirm_id, LanPath::kWifiSta, true);

    // Operator switch: ethernet on 10.0.0.30/24 collides with STA's real 10.0.0.5
    // despite the phone CIDR claiming 192.168.5.0/24.
    harness.ethernet.carrier = true;
    const auto sel = harness.arbiter.SelectLanPath(BothEnabledConfig("10.0.0.30/24", "PhoneNet"),
                                                   "192.168.5.0/24");

    EXPECT_EQ(sel.path, LanPath::kEthernet);  // reconciled to same-subnet, single-path
    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kEthernet);
    EXPECT_GE(harness.uplink.teardown_calls, 1);  // STA suppressed (no dual-home)
}

// R20 mode-independent monitor: an Ethernet hotplug in kP2pOnly (no STA) re-runs
// arbitration and selects Ethernet — the kStaOnly-only carrier watchdog could not.
TEST(ConnectivityArbiterTest, EthernetHotplugInP2pOnlySelectsEthernet) {
    Harness harness;
    harness.ethernet.carrier = false;
    // Ethernet enabled but carrier down, no wifi -> P2P.
    const auto sel = harness.arbiter.SelectLanPath(StaticEthernetConfig("10.10.1.30/24"), "");
    ASSERT_EQ(sel.path, LanPath::kP2p);
    ASSERT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);

    harness.ethernet.carrier = true;  // cable plugged in while kP2pOnly
    harness.monitor.Poll();

    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kEthernet);
    EXPECT_EQ(harness.ethernet.apply_calls, 1);
}

// R20 mode-independent monitor: an Ethernet UNPLUG while LanPath=kEthernet (radio
// kP2pOnly, so the kStaOnly watchdog is idle) re-arbitrates to the next provider.
TEST(ConnectivityArbiterTest, EthernetUnplugWhileCommittedReArbitratesToSta) {
    Harness harness;
    harness.ethernet.carrier = true;
    const auto sel = harness.arbiter.SelectLanPath(BothEnabledConfig("10.10.1.30/24", "PhoneNet"),
                                                   "10.10.1.0/24");
    ASSERT_EQ(sel.path, LanPath::kEthernet);
    harness.arbiter.ConfirmLanPath(sel.confirm_id, LanPath::kEthernet, true);
    ASSERT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kEthernet);

    harness.ethernet.carrier = false;  // cable pulled
    harness.monitor.Poll();

    EXPECT_EQ(harness.ethernet.teardown_calls, 1);
    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);  // fell to STA
}

// R21 DHCP-pending: carrier up, no lease/offer -> not yet a candidate; LAN falls to
// STA (ethernet not applied).
TEST(ConnectivityArbiterTest, DhcpPendingEthernetIsNotACandidate) {
    Harness harness;
    harness.ethernet.carrier = true;
    harness.ethernet.dhcp_offer = std::nullopt;  // no lease yet
    harness.ethernet.probe_mode = sst::network::EthernetMode::kDown;
    harness.ethernet.probe_address = "";

    sst::config::UplinkData cfg;
    cfg.ethernet.enabled = true;
    cfg.ethernet.dhcp = true;
    cfg.wifi.enabled = true;
    cfg.wifi.ssid = "PhoneNet";
    const auto sel = harness.arbiter.SelectLanPath(cfg, "10.10.1.0/24");

    EXPECT_EQ(sel.path, LanPath::kWifiSta);      // fell to STA
    EXPECT_EQ(harness.ethernet.apply_calls, 0);  // ethernet not applied
    EXPECT_GE(harness.ethernet.offer_calls, 1);  // an offer WAS probed (acquire-without-bind)
}

// R21 no-surprise-migrate: a LATE same-subnet DHCP lease onto a HEALTHY STA session
// does NOT auto-migrate (no unsolicited preview drop); the offer is classified
// without binding, so nothing dual-homes.
TEST(ConnectivityArbiterTest, LateDhcpLeaseDoesNotAutoMigrateHealthySta) {
    Harness harness;
    harness.ethernet.carrier = false;  // STA wins first
    sst::config::UplinkData cfg;
    cfg.ethernet.enabled = true;
    cfg.ethernet.dhcp = true;
    cfg.wifi.enabled = true;
    cfg.wifi.ssid = "PhoneNet";
    const auto sta = harness.arbiter.SelectLanPath(cfg, "10.10.1.0/24");
    ASSERT_EQ(sta.path, LanPath::kWifiSta);
    harness.arbiter.ConfirmLanPath(sta.confirm_id, LanPath::kWifiSta, true);
    const int teardowns_before = harness.uplink.teardown_calls;

    // The DHCP lease lands ~30s later on the same subnet — carrier transitions up.
    harness.ethernet.carrier = true;
    harness.ethernet.dhcp_offer = "10.10.1.30/24";
    harness.monitor.Poll();

    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);  // STA kept
    EXPECT_EQ(harness.ethernet.apply_calls, 0);                      // ethernet NOT bound
    EXPECT_EQ(harness.uplink.teardown_calls, teardowns_before);      // STA not suppressed
}

// Acquire-without-bind on an OPERATOR DHCP switch: the STA IP is suppressed BEFORE
// the Ethernet address is bound, so the two are never simultaneously addressed.
TEST(ConnectivityArbiterTest, OperatorDhcpSwitchSuppressesStaBeforeBindingEthernet) {
    Harness harness;
    harness.ethernet.carrier = false;  // STA up first
    sst::config::UplinkData cfg;
    cfg.ethernet.enabled = true;
    cfg.ethernet.dhcp = true;
    cfg.wifi.enabled = true;
    cfg.wifi.ssid = "PhoneNet";
    const auto sta = harness.arbiter.SelectLanPath(cfg, "10.10.1.0/24");
    ASSERT_EQ(sta.path, LanPath::kWifiSta);
    harness.arbiter.ConfirmLanPath(sta.confirm_id, LanPath::kWifiSta, true);
    harness.log.calls.clear();

    // Operator re-selects with the DHCP lease now available on the same subnet.
    harness.ethernet.carrier = true;
    harness.ethernet.dhcp_offer = "10.10.1.30/24";
    const auto sel = harness.arbiter.SelectLanPath(cfg, "10.10.1.0/24");

    EXPECT_EQ(sel.path, LanPath::kEthernet);
    EXPECT_LT(IndexOf(harness.log.calls, "uplink.TeardownWifiSta"),
              IndexOf(harness.log.calls, "ethernet.Apply"));  // no dual-home window
}

// Single-counter epoch: a stale confirm id (wrong epoch) is a no-op.
TEST(ConnectivityArbiterTest, StaleConfirmLanPathIdIsNoOp) {
    Harness harness;
    harness.ethernet.carrier = true;
    const auto sel = harness.arbiter.SelectLanPath(StaticEthernetConfig("10.10.1.30/24"), "");
    ASSERT_EQ(sel.path, LanPath::kEthernet);

    const auto committed =
        harness.arbiter.ConfirmLanPath(sel.confirm_id + 99, LanPath::kEthernet, true);

    EXPECT_EQ(committed, LanPath::kEthernet);  // unchanged (still committing)
    EXPECT_EQ(harness.arbiter.EthernetConfirmId(), sel.confirm_id);
}

// Single-counter epoch: a confirm for the WRONG path (right id) is a no-op.
TEST(ConnectivityArbiterTest, ConfirmLanPathWrongPathIsNoOp) {
    Harness harness;
    harness.ethernet.carrier = true;
    const auto sel = harness.arbiter.SelectLanPath(StaticEthernetConfig("10.10.1.30/24"), "");
    ASSERT_EQ(sel.path, LanPath::kEthernet);

    const auto committed = harness.arbiter.ConfirmLanPath(sel.confirm_id, LanPath::kWifiSta, true);

    EXPECT_EQ(committed, LanPath::kEthernet);  // wrong path -> ignored
}

// The critical single-counter guard: select-Ethernet -> flap-to-STA ->
// re-select-Ethernet. The stale FIRST Ethernet confirm (same path, old id) must NOT
// finalize the superseded pick — the monotonic counter advanced.
TEST(ConnectivityArbiterTest, FlapThenStaleEthernetConfirmIsNoOp) {
    Harness harness;
    harness.ethernet.carrier = true;
    const auto first = harness.arbiter.SelectLanPath(BothEnabledConfig("10.10.1.30/24", "PhoneNet"),
                                                     "10.10.1.0/24");
    ASSERT_EQ(first.path, LanPath::kEthernet);

    // Flap: unplug -> STA, then re-plug -> Ethernet again (a NEW epoch each time).
    harness.ethernet.carrier = false;
    harness.monitor.Poll();  // -> STA (id advances)
    ASSERT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);
    harness.ethernet.carrier = true;
    harness.monitor.Poll();  // -> Ethernet again (id advances again)
    ASSERT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kEthernet);

    const auto live_id = harness.arbiter.EthernetConfirmId();
    EXPECT_GT(live_id, first.confirm_id);

    // The stale FIRST Ethernet confirm (old id, same kEthernet path) is a no-op.
    harness.arbiter.ConfirmLanPath(first.confirm_id, LanPath::kEthernet, true);
    EXPECT_EQ(harness.arbiter.EthernetConfirmId(), live_id);  // unchanged

    // The LIVE confirm commits.
    EXPECT_EQ(harness.arbiter.ConfirmLanPath(live_id, LanPath::kEthernet, true),
              LanPath::kEthernet);
}

// ─────────────────────── U6: Ethernet-LAN confirmation handshake ───────────────

// AE8: Ethernet-only on a shared LAN -> the arbiter mints a per-path credential (via
// the transition hook) + an endpoint on the Ethernet IP; ConfirmLanPath(eth_id,
// kEthernet, true) commits kEthernet and the surfaced credential is NON-EMPTY.
TEST(ConnectivityArbiterTest, EthernetLanHandshakeMintsCredentialAndCommitsNonEmpty) {
    Harness harness;
    harness.hooks.ethernet_credential = "cam:ethsecret";  // U6: hook mints the RTSP cred
    harness.ethernet.carrier = true;

    const auto sel = harness.arbiter.SelectLanPath(StaticEthernetConfig("10.10.1.30/24"), "");

    ASSERT_EQ(sel.path, LanPath::kEthernet);
    EXPECT_EQ(harness.hooks.ethernet_selected_calls, 1);
    EXPECT_EQ(harness.hooks.last_ethernet_ip, "10.10.1.30");  // hook got the bare Ethernet IP
    EXPECT_EQ(sel.endpoint, "10.10.1.30");
    EXPECT_EQ(sel.credential, "cam:ethsecret");  // handshake carries the cred
    EXPECT_EQ(harness.arbiter.EthernetCredential(), "cam:ethsecret");
    EXPECT_NE(sel.confirm_id, 0U);

    const auto committed = harness.arbiter.ConfirmLanPath(sel.confirm_id, LanPath::kEthernet, true);

    EXPECT_EQ(committed, LanPath::kEthernet);
    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kEthernet);
    // Credential survives the commit (still the live, non-empty per-path secret).
    EXPECT_EQ(harness.arbiter.EthernetCredential(), "cam:ethsecret");
    EXPECT_EQ(harness.arbiter.EthernetConfirmId(), sel.confirm_id);
}

// R19: preview NOT reachable on the Ethernet IP (probe fails) -> de-select, fall to
// STA, and the Ethernet credential + endpoint are CLEARED (they reflect only the
// live selection — a de-selected path exposes no stale secret).
TEST(ConnectivityArbiterTest, EthernetProbeFailDeselectsAndClearsCredential) {
    Harness harness;
    harness.hooks.ethernet_credential = "cam:ethsecret";
    harness.ethernet.carrier = true;

    const auto sel = harness.arbiter.SelectLanPath(BothEnabledConfig("10.10.1.30/24", "PhoneNet"),
                                                   "10.10.1.0/24");
    ASSERT_EQ(sel.path, LanPath::kEthernet);
    ASSERT_EQ(harness.arbiter.EthernetCredential(), "cam:ethsecret");

    const auto committed =
        harness.arbiter.ConfirmLanPath(sel.confirm_id, LanPath::kEthernet, false);

    EXPECT_EQ(committed, LanPath::kWifiSta);              // fell to STA (R19)
    EXPECT_EQ(harness.ethernet.teardown_calls, 1);        // Ethernet torn down
    EXPECT_EQ(harness.arbiter.EthernetCredential(), "");  // stale secret cleared
    EXPECT_EQ(harness.arbiter.EthernetEndpoint(), "");
    EXPECT_EQ(harness.arbiter.EthernetConfirmId(), 0U);  // not kEthernet -> no handshake
}

// The credential/endpoint reflect the LIVE selection only: after a flap that
// supersedes the first Ethernet pick, a stale confirm for the OLD id is a no-op and
// the surfaced credential remains the live one (no resurrection of a superseded pick).
TEST(ConnectivityArbiterTest, SupersededEthernetConfirmKeepsLiveCredential) {
    Harness harness;
    harness.hooks.ethernet_credential = "cam:ethsecret";
    harness.ethernet.carrier = true;

    const auto first = harness.arbiter.SelectLanPath(BothEnabledConfig("10.10.1.30/24", "PhoneNet"),
                                                     "10.10.1.0/24");
    ASSERT_EQ(first.path, LanPath::kEthernet);

    // Flap: unplug -> STA (id advances, Ethernet cred cleared), re-plug -> Ethernet
    // again (new id, cred re-minted).
    harness.ethernet.carrier = false;
    harness.monitor.Poll();
    ASSERT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);
    harness.ethernet.carrier = true;
    harness.monitor.Poll();
    ASSERT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kEthernet);
    const auto live_id = harness.arbiter.EthernetConfirmId();
    ASSERT_GT(live_id, first.confirm_id);

    // Stale confirm for the FIRST (superseded) Ethernet id is a no-op; the live cred
    // is untouched.
    harness.arbiter.ConfirmLanPath(first.confirm_id, LanPath::kEthernet, false);
    EXPECT_EQ(harness.arbiter.EthernetConfirmId(), live_id);
    EXPECT_EQ(harness.arbiter.EthernetCredential(), "cam:ethsecret");
    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kEthernet);
}

// ─────────────────────── F1-F7 code-review-fix regressions ────────────────────

// F1: a different-subnet uplink established via the ARBITER (a monitor poll, not the
// boot path) BINDS the address (a real ApplyEthernet) and installs the uplink policy
// routing AGAINST the bound address — the routing config carries the bound CIDR,
// proving the address was addressed BEFORE the routing was synced. LAN stays STA.
TEST(ConnectivityArbiterTest, F1DifferentSubnetUplinkBindsAddressBeforeRouting) {
    Harness harness;
    harness.uplink.sta_result = {.ok = true, .detail = "10.10.1.142"};
    harness.uplink.probe_result = {.ok = true, .detail = "10.10.1.142"};
    harness.ethernet.carrier = false;  // STA wins the LAN first
    const auto sta = harness.arbiter.SelectLanPath(BothEnabledConfig("192.168.9.30/24", "PhoneNet"),
                                                   "10.10.1.0/24");
    ASSERT_EQ(sta.path, LanPath::kWifiSta);
    harness.arbiter.ConfirmLanPath(sta.confirm_id, LanPath::kWifiSta, true);

    // Ethernet appears on a different subnet WITH a gateway.
    harness.ethernet.carrier = true;
    harness.ethernet.probe_address = "192.168.9.30/24";
    harness.ethernet.gateway = "192.168.9.1";
    harness.monitor.Poll();

    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);  // STA still the LAN (R4)
    EXPECT_EQ(harness.ethernet.apply_calls, 1);        // F1: the uplink address was BOUND
    EXPECT_EQ(harness.policy_routing.apply_calls, 1);  // routing installed once
    // Proof the routing was installed AGAINST the bound (re-probed) address, i.e. the
    // bind happened first — the config carries the bound CIDR, not an empty/unbound one.
    EXPECT_EQ(harness.policy_routing.last_cfg.uplink_address_cidr, "192.168.9.30/24");
    EXPECT_EQ(harness.policy_routing.last_cfg.uplink_gateway, "192.168.9.1");
}

// F1 guard: if the different-subnet uplink address FAILS to bind, no routing is
// installed against an unbound address — the apply is skipped (and the stale table
// torn down), never applied against nothing. LAN stays STA.
TEST(ConnectivityArbiterTest, F1UnboundUplinkDoesNotInstallRouting) {
    Harness harness;
    harness.uplink.sta_result = {.ok = true, .detail = "10.10.1.142"};
    harness.uplink.probe_result = {.ok = true, .detail = "10.10.1.142"};
    harness.ethernet.carrier = false;
    const auto sta = harness.arbiter.SelectLanPath(BothEnabledConfig("192.168.9.30/24", "PhoneNet"),
                                                   "10.10.1.0/24");
    ASSERT_EQ(sta.path, LanPath::kWifiSta);
    harness.arbiter.ConfirmLanPath(sta.confirm_id, LanPath::kWifiSta, true);

    harness.ethernet.apply_ok = false;  // the uplink address cannot be bound
    harness.ethernet.carrier = true;
    harness.ethernet.probe_address = "192.168.9.30/24";
    harness.ethernet.gateway = "192.168.9.1";
    harness.monitor.Poll();

    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);  // STA unaffected
    EXPECT_EQ(harness.ethernet.apply_calls, 1);                      // a bind WAS attempted
    EXPECT_EQ(harness.policy_routing.apply_calls, 0);  // but routing NOT installed unbound
}

// F3: an Ethernet-hotplug arbitration resolves the (potentially 20s dhclient) probe
// WITHOUT holding `mtx_`. A hook fired inside the off-lock ProbeEthernet re-enters a
// BLE-facing getter (CurrentLanPath); that it returns (no self-deadlock) proves the
// arbiter lock was dropped across the shell-out — the whole point of F3.
TEST(ConnectivityArbiterTest, F3ProbeDoesNotHoldLockAcrossShellOut) {
    Harness harness;
    harness.ethernet.carrier = false;
    const auto sel = harness.arbiter.SelectLanPath(StaticEthernetConfig("10.10.1.30/24"), "");
    ASSERT_EQ(sel.path, LanPath::kP2p);

    int reentrant_getter_calls = 0;
    harness.ethernet.on_probe = [&] {
        // Would DEADLOCK on a non-recursive mutex if the arbiter held `mtx_` here.
        (void)harness.arbiter.CurrentLanPath();
        ++reentrant_getter_calls;
    };

    harness.ethernet.carrier = true;  // hotplug -> off-lock probe path
    harness.monitor.Poll();

    EXPECT_GT(reentrant_getter_calls, 0);  // getter stayed serviceable during the probe
    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kEthernet);  // and arbitration still ran
}

// F4: a DHCP-pending Ethernet (carrier up, no lease yet) that later gets a lease is
// promoted WITHOUT any carrier edge — the monitor re-arbitrates on the pending flag,
// not only on an up/down transition.
TEST(ConnectivityArbiterTest, F4LateDhcpLeasePromotesWithoutCarrierEdge) {
    Harness harness;
    sst::config::UplinkData cfg;
    cfg.ethernet.enabled = true;
    cfg.ethernet.dhcp = true;
    cfg.wifi.enabled = false;  // no STA — promotion falls out cleanly to Ethernet
    // Carrier already up but no offer yet: DHCP-pending.
    harness.ethernet.carrier = true;
    harness.ethernet.probe_mode = sst::network::EthernetMode::kDown;
    harness.ethernet.probe_address = "";
    harness.ethernet.dhcp_offer = std::nullopt;
    const auto sel = harness.arbiter.SelectLanPath(cfg, "");
    ASSERT_EQ(sel.path, LanPath::kP2p);          // pending -> not yet a candidate
    ASSERT_EQ(harness.ethernet.apply_calls, 0);  // nothing bound yet

    // The lease lands later. Carrier stays UP the whole time (NO edge).
    harness.ethernet.dhcp_offer = "10.10.1.30/24";
    harness.monitor.Poll();  // carrier unchanged == no edge; re-polls on the pending flag

    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kEthernet);  // promoted
    EXPECT_EQ(harness.ethernet.apply_calls, 1);                       // Ethernet now bound
    EXPECT_GE(harness.ethernet.offer_calls, 2);  // the offer was re-polled after the pending hold
}

// F5: the R19 Ethernet->STA fallback — ConfirmLanPath(kEthernet, false) with a
// reachable STA — mints a FRESH STA epoch that stays retrievable (CurrentMigrationId)
// so the app can ConfirmPreviewReachable it before the self-revert timer collapses to
// P2P. Without F5 that migration id would be stranded (the confirm returns only a path).
TEST(ConnectivityArbiterTest, F5EthernetToStaFallbackDeliversUsableMigrationId) {
    Harness harness;
    harness.uplink.sta_result = {.ok = true, .detail = "10.0.0.9"};
    harness.ethernet.carrier = true;
    const auto sel = harness.arbiter.SelectLanPath(BothEnabledConfig("10.10.1.30/24", "PhoneNet"),
                                                   "10.10.1.0/24");
    ASSERT_EQ(sel.path, LanPath::kEthernet);

    // Preview NOT reachable on the Ethernet IP -> de-select, fall to STA (R19).
    const auto committed =
        harness.arbiter.ConfirmLanPath(sel.confirm_id, LanPath::kEthernet, false);

    ASSERT_EQ(committed, LanPath::kWifiSta);
    EXPECT_EQ(harness.arbiter.CurrentMode(), ConnectivityMode::kMigrating);
    const auto fallback_id = harness.arbiter.CurrentMigrationId();
    EXPECT_NE(fallback_id, 0U);              // F5: a usable migration id survived the fallback
    EXPECT_GT(fallback_id, sel.confirm_id);  // a FRESH epoch, not the retired Ethernet one
    // And it is genuinely usable: confirming it finalises kStaOnly (not lost to revert).
    EXPECT_EQ(harness.arbiter.ConfirmSharedWifi(fallback_id, true), ConnectivityMode::kStaOnly);
}

// F6: a PARTIALLY-failed ApplyUplinkRouting still creates the nft drop table, so a
// later transition-away MUST attempt teardown — gated on "touched", never on the
// success flag. A regressed F6 (gate on active_) would skip teardown and leak the
// drop table, firewalling a later LAN.
TEST(ConnectivityArbiterTest, F6PartialUplinkApplyIsTornDownOnTransitionAway) {
    Harness harness;
    harness.uplink.sta_result = {.ok = true, .detail = "10.10.1.142"};
    harness.uplink.probe_result = {.ok = true, .detail = "10.10.1.142"};
    harness.ethernet.carrier = false;
    const auto sta = harness.arbiter.SelectLanPath(BothEnabledConfig("192.168.9.30/24", "PhoneNet"),
                                                   "10.10.1.0/24");
    ASSERT_EQ(sta.path, LanPath::kWifiSta);
    harness.arbiter.ConfirmLanPath(sta.confirm_id, LanPath::kWifiSta, true);

    // Different-subnet uplink appears, but the apply PARTIALLY fails (returns false).
    harness.policy_routing.apply_ok = false;
    harness.ethernet.carrier = true;
    harness.ethernet.probe_address = "192.168.9.30/24";
    harness.ethernet.gateway = "192.168.9.1";
    harness.monitor.Poll();
    ASSERT_EQ(harness.policy_routing.apply_calls, 1);     // apply attempted (touched the table)
    ASSERT_EQ(harness.policy_routing.teardown_calls, 0);  // nothing torn down yet

    // Cable pulled -> transition away. Even though the apply reported FAILURE, the
    // (possibly leaked) drop table must be torn down.
    harness.ethernet.carrier = false;
    harness.monitor.Poll();

    EXPECT_EQ(harness.policy_routing.teardown_calls, 1);  // F6: torn down despite active_==false
    EXPECT_EQ(harness.arbiter.CurrentLanPath(), LanPath::kWifiSta);
}

}  // namespace
