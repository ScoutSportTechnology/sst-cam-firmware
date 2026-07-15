#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "app/control/ports/wifi-manager.hpp"
#include "app/network/ports/connectivity-arbiter.hpp"
#include "app/network/ports/self-revert-timer.hpp"
#include "app/network/ports/uplink-configurator.hpp"
#include "app/network/services/connectivity_arbiter/connectivity-arbiter.hpp"
#include "domain/config/models/uplink-config.hpp"
#include "domain/network/models/connectivity-mode.hpp"

namespace {

using sst::network::ConnectivityMode;

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
    auto ApplyEthernet(const sst::config::EthernetUplink& /*cfg*/)
        -> sst::network::UplinkResult override {
        return {.ok = true, .detail = ""};
    }
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
    auto ProbeEthernet() -> sst::network::UplinkResult override {
        return {.ok = false, .detail = ""};
    }
    auto ProbeWifiSta() -> sst::network::UplinkResult override {
        return {.ok = false, .detail = ""};
    }

    sst::network::UplinkResult sta_result{.ok = true, .detail = "10.0.0.5"};
    int wifi_calls{0};
    int teardown_calls{0};

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

    std::string credential{"cam:secret"};
    std::string last_sta_ip;
    int joined_calls{0};
    int reverted_calls{0};
    bool revert_ok{true};

   private:
    CallLog& log_;
};

struct Harness {
    CallLog log;
    FakeWifi wifi{log};
    FakeUplink uplink{log};
    FakeTimer timer{log};
    FakeHooks hooks{log};
    sst::network::ConnectivityArbiter arbiter{wifi, uplink, timer, hooks,
                                              std::chrono::milliseconds{5000}};
};

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
    EXPECT_FALSE(harness.timer.armed());
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

}  // namespace
