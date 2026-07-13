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
    Harness h;
    EXPECT_EQ(h.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
    EXPECT_TRUE(h.log.calls.empty());
    EXPECT_EQ(h.uplink.wifi_calls, 0);
}

// Happy + ordering (AE2/AE5): GO stopped BEFORE STA applied, lands kMigrating.
TEST(ConnectivityArbiterTest, RequestSharedWifiStopsGoBeforeApplyingSta) {
    Harness h;
    sst::config::WifiStaUplink cfg;
    cfg.ssid = "PhoneNet";

    const auto migration = h.arbiter.RequestSharedWifi(cfg);

    ASSERT_TRUE(migration.has_value());
    EXPECT_EQ(migration->id, 1U);
    EXPECT_EQ(migration->preview_endpoint, "10.0.0.5");
    EXPECT_EQ(migration->preview_credential, "cam:secret");
    EXPECT_EQ(h.arbiter.CurrentMode(), ConnectivityMode::kMigrating);

    const int stop_idx = IndexOf(h.log.calls, "wifi.Stop");
    const int apply_idx = IndexOf(h.log.calls, "uplink.ApplyWifiSta");
    ASSERT_GE(stop_idx, 0);
    ASSERT_GE(apply_idx, 0);
    EXPECT_LT(stop_idx, apply_idx) << "INV2: GO teardown must precede STA join";
    EXPECT_TRUE(h.timer.armed());
}

// STA join fails → arbiter reverts to a legal S1, returns nullopt.
TEST(ConnectivityArbiterTest, StaJoinFailureRevertsToP2p) {
    Harness h;
    h.uplink.sta_result = {.ok = false, .detail = "wrong passphrase"};
    sst::config::WifiStaUplink cfg;

    const auto migration = h.arbiter.RequestSharedWifi(cfg);

    EXPECT_FALSE(migration.has_value());
    EXPECT_EQ(h.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
    EXPECT_EQ(h.hooks.reverted_calls, 1);
    EXPECT_EQ(h.uplink.teardown_calls, 1);  // STA cleanly torn down on revert
    EXPECT_FALSE(h.timer.armed());
    // Still ordered: Stop happened, ApplyWifiSta happened, then revert.
    EXPECT_LT(IndexOf(h.log.calls, "wifi.Stop"), IndexOf(h.log.calls, "uplink.ApplyWifiSta"));
}

// Confirm epoch (AE2): in-epoch reachable confirm → kStaOnly, timer disarmed.
TEST(ConnectivityArbiterTest, InEpochConfirmFinalisesStaOnly) {
    Harness h;
    sst::config::WifiStaUplink cfg;
    const auto migration = h.arbiter.RequestSharedWifi(cfg);
    ASSERT_TRUE(migration.has_value());

    const auto mode = h.arbiter.ConfirmSharedWifi(migration->id, true);

    EXPECT_EQ(mode, ConnectivityMode::kStaOnly);
    EXPECT_EQ(h.arbiter.CurrentMode(), ConnectivityMode::kStaOnly);
    EXPECT_FALSE(h.timer.armed());
    EXPECT_EQ(h.hooks.reverted_calls, 0);
}

// Stale confirm id → ignored, no transition.
TEST(ConnectivityArbiterTest, StaleConfirmIsIgnored) {
    Harness h;
    sst::config::WifiStaUplink cfg;
    const auto migration = h.arbiter.RequestSharedWifi(cfg);
    ASSERT_TRUE(migration.has_value());

    const auto mode = h.arbiter.ConfirmSharedWifi(migration->id + 99, true);

    EXPECT_EQ(mode, ConnectivityMode::kMigrating);  // unchanged
    EXPECT_EQ(h.arbiter.CurrentMode(), ConnectivityMode::kMigrating);
    EXPECT_TRUE(h.timer.armed());
}

// Deny (isolation, AE3/AE5): in-epoch not-reachable → revert to kP2pOnly.
TEST(ConnectivityArbiterTest, DenyRevertsToP2p) {
    Harness h;
    sst::config::WifiStaUplink cfg;
    const auto migration = h.arbiter.RequestSharedWifi(cfg);
    ASSERT_TRUE(migration.has_value());

    const auto mode = h.arbiter.ConfirmSharedWifi(migration->id, false);

    EXPECT_EQ(mode, ConnectivityMode::kP2pOnly);
    EXPECT_EQ(h.hooks.reverted_calls, 1);
    EXPECT_EQ(h.uplink.teardown_calls, 1);  // STA cleanly torn down on revert
    EXPECT_FALSE(h.timer.armed());
}

// Re-entrancy (AE5): a second request while kMigrating is rejected — the radio is
// never driven a second time concurrently.
TEST(ConnectivityArbiterTest, ReentrantRequestWhileMigratingIsRejected) {
    Harness h;
    sst::config::WifiStaUplink cfg;
    ASSERT_TRUE(h.arbiter.RequestSharedWifi(cfg).has_value());

    const auto second = h.arbiter.RequestSharedWifi(cfg);

    EXPECT_FALSE(second.has_value());
    EXPECT_EQ(h.uplink.wifi_calls, 1);  // STA applied exactly once, not twice
    EXPECT_EQ(h.arbiter.CurrentMode(), ConnectivityMode::kMigrating);
}

// A session finalisation (OnWifiStopped) modelled as RevertToP2p mid-migration →
// ends in a legal state.
TEST(ConnectivityArbiterTest, RevertMidMigrationEndsLegal) {
    Harness h;
    sst::config::WifiStaUplink cfg;
    ASSERT_TRUE(h.arbiter.RequestSharedWifi(cfg).has_value());

    h.arbiter.RevertToP2p();

    EXPECT_EQ(h.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
    EXPECT_EQ(h.hooks.reverted_calls, 1);
    EXPECT_EQ(h.uplink.teardown_calls, 1);  // STA cleanly torn down on revert
    EXPECT_FALSE(h.timer.armed());
}

// Self-revert timeout with no confirm → kP2pOnly.
TEST(ConnectivityArbiterTest, SelfRevertTimeoutRevertsToP2p) {
    Harness h;
    sst::config::WifiStaUplink cfg;
    ASSERT_TRUE(h.arbiter.RequestSharedWifi(cfg).has_value());

    h.timer.Fire();  // simulate the backstop expiring with no app confirm

    EXPECT_EQ(h.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
    EXPECT_EQ(h.hooks.reverted_calls, 1);
    EXPECT_EQ(h.uplink.teardown_calls, 1);  // STA cleanly torn down on revert
}

// A late confirm after the self-revert already fired (stale id) → no re-entry.
TEST(ConnectivityArbiterTest, LateConfirmAfterSelfRevertDoesNotReenterSta) {
    Harness h;
    sst::config::WifiStaUplink cfg;
    const auto migration = h.arbiter.RequestSharedWifi(cfg);
    ASSERT_TRUE(migration.has_value());

    h.timer.Fire();  // self-reverts to kP2pOnly, migration id retired
    const auto mode = h.arbiter.ConfirmSharedWifi(migration->id, true);

    EXPECT_EQ(mode, ConnectivityMode::kP2pOnly);
    EXPECT_EQ(h.arbiter.CurrentMode(), ConnectivityMode::kP2pOnly);
}

// After finalising kStaOnly, a fresh migration mints a NEW id (monotonic epochs).
TEST(ConnectivityArbiterTest, MigrationIdsAreMonotonic) {
    Harness h;
    sst::config::WifiStaUplink cfg;
    const auto first = h.arbiter.RequestSharedWifi(cfg);
    ASSERT_TRUE(first.has_value());
    h.arbiter.ConfirmSharedWifi(first->id, false);  // back to P2P

    const auto second = h.arbiter.RequestSharedWifi(cfg);
    ASSERT_TRUE(second.has_value());
    EXPECT_GT(second->id, first->id);
}

}  // namespace
