#include <gtest/gtest.h>

#include <algorithm>
#include <optional>

#include "app/control/services/handlers/network.handler.hpp"
#include "app/network/ports/uplink-configurator.hpp"
#include "app/network/ports/uplink-store.hpp"
#include "app/network/services/uplink-manager/uplink-manager.hpp"
#include "bluetooth.pb.h"
#include "domain/config/models/uplink-config.hpp"

namespace {

// Configurator whose ethernet apply outcome is parameterizable, so a test can
// drive both the success and the apply-failure path.
class FakeConfigurator final : public sst::network::IUplinkConfigurator {
   public:
    explicit FakeConfigurator(bool ethernet_apply_ok = true)
        : ethernet_apply_ok_(ethernet_apply_ok) {}

    auto ApplyEthernet(const sst::config::EthernetUplink& /*cfg*/)
        -> sst::network::UplinkResult override {
        ++ethernet_calls;
        return ethernet_apply_ok_
                   ? sst::network::UplinkResult{.ok = true, .detail = "10.10.1.30/24"}
                   : sst::network::UplinkResult{.ok = false, .detail = "nmcli con up failed"};
    }
    auto ApplyWifiSta(const sst::config::WifiStaUplink& /*cfg*/)
        -> sst::network::UplinkResult override {
        return {.ok = false, .detail = "unavailable"};
    }
    auto TeardownWifiSta() -> void override {}
    auto ProbeEthernet() -> sst::network::UplinkResult override {
        return {.ok = true, .detail = "10.10.1.30/24"};
    }
    auto ProbeWifiSta() -> sst::network::UplinkResult override {
        return {.ok = false, .detail = "unavailable"};
    }
    int ethernet_calls{0};

   private:
    bool ethernet_apply_ok_;
};

// In-memory store: records what (if anything) was persisted, so a test can
// assert a bad config is NOT persisted as authoritative.
class FakeStore final : public sst::network::IUplinkStore {
   public:
    auto Persist(const sst::config::UplinkData& data) -> bool override {
        ++persist_calls;
        last = data;
        return true;
    }
    int persist_calls{0};
    std::optional<sst::config::UplinkData> last;
};

// Hand-written fake credential-link security predicate (no GMock). Parameterized
// so a test can drive both the encrypted and the cleartext link (INV6/AE7).
class FakeLinkSecurity final : public sst::control::ILinkSecurity {
   public:
    explicit FakeLinkSecurity(bool secure) : secure_(secure) {}
    [[nodiscard]] auto IsCredentialLinkSecure() const -> bool override { return secure_; }

   private:
    bool secure_;
};

// Fake connectivity arbiter: records the wifi routing (request/confirm/revert)
// and returns a parameterizable migration so the handler's shared-wifi path is
// observable without the real radio.
class FakeArbiter final : public sst::network::IConnectivityArbiter {
   public:
    auto RequestSharedWifi(const sst::config::WifiStaUplink& /*uplink*/)
        -> std::optional<sst::network::SharedWifiMigration> override {
        ++request_calls;
        if (!request_ok) {
            return std::nullopt;
        }
        return sst::network::SharedWifiMigration{
            .id = 7, .preview_endpoint = "10.0.0.9", .preview_credential = "cam:secret"};
    }
    auto ConfirmSharedWifi(sst::network::MigrationId migration_id,
                           bool reachable) -> sst::network::ConnectivityMode override {
        ++confirm_calls;
        last_confirm_id = migration_id;
        last_reachable = reachable;
        return reachable ? sst::network::ConnectivityMode::kStaOnly
                         : sst::network::ConnectivityMode::kP2pOnly;
    }
    auto RevertToP2p() -> void override { ++revert_calls; }
    [[nodiscard]] auto CurrentMode() const -> sst::network::ConnectivityMode override {
        return sst::network::ConnectivityMode::kP2pOnly;
    }

    bool request_ok{true};
    int request_calls{0};
    int confirm_calls{0};
    int revert_calls{0};
    sst::network::MigrationId last_confirm_id{0};
    bool last_reachable{false};
};

// Build a SetNetworkConfig command that ENABLES ethernet with a valid static
// address, so the apply/persist path is observable via the fake configurator's
// ethernet_calls.
auto MakeSetEthernetCommand() -> sst_cam::Command {
    sst_cam::Command cmd;
    auto* eth = cmd.mutable_set_network_config()->mutable_config()->mutable_ethernet();
    eth->set_enabled(true);
    eth->set_dhcp(false);
    eth->set_address("10.10.1.30/24");
    eth->set_gateway("10.10.1.1");
    return cmd;
}

TEST(NetworkHandlerTest, HandlesSetAndGet) {
    FakeConfigurator configurator;
    sst::network::UplinkManager manager(configurator);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter);

    const auto cases = handler.HandledCases();
    EXPECT_NE(std::find(cases.begin(), cases.end(), sst_cam::Command::kSetNetworkConfig),
              cases.end());
    EXPECT_NE(std::find(cases.begin(), cases.end(), sst_cam::Command::kGetNetworkConfig),
              cases.end());
}

TEST(NetworkHandlerTest, SetAppliesPersistsAndEchoesStatus) {
    FakeConfigurator configurator;
    sst::network::UplinkManager manager(configurator);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter);

    sst_cam::Command cmd;
    auto* eth = cmd.mutable_set_network_config()->mutable_config()->mutable_ethernet();
    eth->set_enabled(true);
    eth->set_dhcp(false);
    eth->set_address("10.10.1.30/24");
    eth->set_gateway("10.10.1.1");

    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kNetworkConfig);
    // Config echoed back.
    const auto& echoed = resp.network_config().config().ethernet();
    EXPECT_TRUE(echoed.enabled());
    EXPECT_FALSE(echoed.dhcp());
    EXPECT_EQ(echoed.address(), "10.10.1.30/24");
    EXPECT_EQ(echoed.gateway(), "10.10.1.1");
    // Live status (probed, not the apply result).
    EXPECT_TRUE(resp.network_config().ethernet_up());
    EXPECT_EQ(resp.network_config().ethernet_address(), "10.10.1.30/24");
    // Applied + persisted (apply succeeded).
    EXPECT_EQ(configurator.ethernet_calls, 1);
    EXPECT_EQ(store.persist_calls, 1);
}

// Apply fails for an ENABLED ethernet interface -> the response must NOT be a
// plain OK/green, and the bad config must NOT be persisted as authoritative.
TEST(NetworkHandlerTest, SetWithFailedApplyReportsErrorAndDoesNotPersist) {
    FakeConfigurator configurator(/*ethernet_apply_ok=*/false);
    sst::network::UplinkManager manager(configurator);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter);

    sst_cam::Command cmd;
    auto* eth = cmd.mutable_set_network_config()->mutable_config()->mutable_ethernet();
    eth->set_enabled(true);
    eth->set_dhcp(false);
    eth->set_address("10.10.1.30/24");
    eth->set_gateway("192.168.99.1");  // off-subnet -> con up fails

    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(configurator.ethernet_calls, 1);              // it did attempt the apply
    EXPECT_NE(resp.status(), sst_cam::ResponseStatus::OK);  // not a false green
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_EQ(store.persist_calls, 0);  // bad config not persisted as good
}

TEST(NetworkHandlerTest, GetReturnsInitialConfigWithoutApplying) {
    FakeConfigurator configurator;
    sst::network::UplinkManager manager(configurator);
    FakeStore store;
    FakeArbiter arbiter;

    sst::config::UplinkData initial;
    initial.ethernet.enabled = true;
    initial.ethernet.address = "192.168.0.5/24";

    sst::control::NetworkHandler handler(manager, store, initial, arbiter);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kNetworkConfig);
    EXPECT_TRUE(resp.network_config().config().ethernet().enabled());
    EXPECT_EQ(resp.network_config().config().ethernet().address(), "192.168.0.5/24");
    EXPECT_TRUE(resp.network_config().ethernet_up());
    EXPECT_EQ(configurator.ethernet_calls, 0);  // Get does not re-apply
}

// AE7: SetNetworkConfig arriving on an unencrypted/unbonded link is rejected —
// ERROR, the uplink is NOT applied, and nothing is persisted (the passphrase
// never crosses cleartext).
TEST(NetworkHandlerTest, SetOnInsecureLinkRejectedNotAppliedNotPersisted) {
    FakeConfigurator configurator;
    sst::network::UplinkManager manager(configurator);
    FakeStore store;
    FakeArbiter arbiter;
    FakeLinkSecurity insecure(/*secure=*/false);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, &insecure);

    const auto resp = handler.Handle(MakeSetEthernetCommand());

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_FALSE(resp.error_message().empty());  // a clear reason for the app
    EXPECT_EQ(configurator.ethernet_calls, 0);   // apply never attempted
    EXPECT_EQ(store.persist_calls, 0);           // credentials not persisted
}

// AE7 complement: on an encrypted/bonded link the credential command proceeds
// exactly as before — applied and persisted.
TEST(NetworkHandlerTest, SetOnSecureLinkProceedsAppliesAndPersists) {
    FakeConfigurator configurator;
    sst::network::UplinkManager manager(configurator);
    FakeStore store;
    FakeArbiter arbiter;
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, &secure);

    const auto resp = handler.Handle(MakeSetEthernetCommand());

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(configurator.ethernet_calls, 1);  // applied
    EXPECT_EQ(store.persist_calls, 1);          // persisted
}

// GetNetworkConfig is not credential-bearing, so the link-security gate must not
// affect it even on an insecure link (reconnect/status UX preserved).
TEST(NetworkHandlerTest, GetUnaffectedByInsecureLink) {
    FakeConfigurator configurator;
    sst::network::UplinkManager manager(configurator);
    FakeStore store;
    FakeArbiter arbiter;
    FakeLinkSecurity insecure(/*secure=*/false);

    sst::config::UplinkData initial;
    initial.ethernet.enabled = true;
    initial.ethernet.address = "192.168.0.5/24";
    sst::control::NetworkHandler handler(manager, store, initial, arbiter, &insecure);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kNetworkConfig);
    EXPECT_TRUE(resp.network_config().config().ethernet().enabled());
    EXPECT_EQ(resp.network_config().config().ethernet().address(), "192.168.0.5/24");
    EXPECT_EQ(configurator.ethernet_calls, 0);  // Get never applies
}

// Enabling WiFi routes through the arbiter (never the manager directly, so INV2
// holds) and the reply carries the migration id / STA endpoint / credential.
TEST(NetworkHandlerTest, SetWithWifiBeginsMigrationAndReportsEndpoint) {
    FakeConfigurator configurator;
    sst::network::UplinkManager manager(configurator);
    FakeStore store;
    FakeArbiter arbiter;
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, &secure);

    sst_cam::Command cmd;
    auto* wifi = cmd.mutable_set_network_config()->mutable_config()->mutable_wifi();
    wifi->set_enabled(true);
    wifi->set_ssid("PhoneNet");
    wifi->set_password("hunter2pw");

    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(arbiter.request_calls, 1);  // wifi routed to the arbiter
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kNetworkConfig);
    EXPECT_EQ(resp.network_config().migration_id(), 7U);
    EXPECT_EQ(resp.network_config().preview_endpoint(), "10.0.0.9");
    EXPECT_EQ(resp.network_config().preview_credential(), "cam:secret");
}

// A shared-wifi migration that cannot start (arbiter returns nullopt) -> ERROR,
// nothing persisted.
TEST(NetworkHandlerTest, SetWithWifiMigrationFailureReportsError) {
    FakeConfigurator configurator;
    sst::network::UplinkManager manager(configurator);
    FakeStore store;
    FakeArbiter arbiter;
    arbiter.request_ok = false;
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, &secure);

    sst_cam::Command cmd;
    auto* wifi = cmd.mutable_set_network_config()->mutable_config()->mutable_wifi();
    wifi->set_enabled(true);
    wifi->set_ssid("PhoneNet");

    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_EQ(arbiter.request_calls, 1);
    EXPECT_EQ(store.persist_calls, 0);
}

// Disabling wifi ensures a revert to S1 (never leaves a dangling STA request).
TEST(NetworkHandlerTest, SetWithWifiDisabledRevertsToP2p) {
    FakeConfigurator configurator;
    sst::network::UplinkManager manager(configurator);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter);

    const auto resp = handler.Handle(MakeSetEthernetCommand());  // wifi disabled

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(arbiter.request_calls, 0);
    EXPECT_EQ(arbiter.revert_calls, 1);
}

// ConfirmPreviewReachable routes the app verdict to the arbiter and echoes the
// settled mode.
TEST(NetworkHandlerTest, ConfirmPreviewReachableRoutesToArbiter) {
    FakeConfigurator configurator;
    sst::network::UplinkManager manager(configurator);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter);

    EXPECT_NE(std::find(handler.HandledCases().begin(), handler.HandledCases().end(),
                        sst_cam::Command::kConfirmPreviewReachable),
              handler.HandledCases().end());

    sst_cam::Command cmd;
    auto* confirm = cmd.mutable_confirm_preview_reachable();
    confirm->set_migration_id(7);
    confirm->set_reachable(true);

    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(arbiter.confirm_calls, 1);
    EXPECT_EQ(arbiter.last_confirm_id, 7U);
    EXPECT_TRUE(arbiter.last_reachable);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kConfirmPreviewReachable);
    EXPECT_EQ(resp.confirm_preview_reachable().migration_id(), 7U);
    EXPECT_TRUE(resp.confirm_preview_reachable().in_shared_wifi());
}

}  // namespace
