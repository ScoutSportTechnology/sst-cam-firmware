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
    sst::control::NetworkHandler handler(manager, store, {});

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
    sst::control::NetworkHandler handler(manager, store, {});

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
    sst::control::NetworkHandler handler(manager, store, {});

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

    sst::config::UplinkData initial;
    initial.ethernet.enabled = true;
    initial.ethernet.address = "192.168.0.5/24";

    sst::control::NetworkHandler handler(manager, store, initial);

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
    FakeLinkSecurity insecure(/*secure=*/false);
    sst::control::NetworkHandler handler(manager, store, {}, &insecure);

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
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, &secure);

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
    FakeLinkSecurity insecure(/*secure=*/false);

    sst::config::UplinkData initial;
    initial.ethernet.enabled = true;
    initial.ethernet.address = "192.168.0.5/24";
    sst::control::NetworkHandler handler(manager, store, initial, &insecure);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kNetworkConfig);
    EXPECT_TRUE(resp.network_config().config().ethernet().enabled());
    EXPECT_EQ(resp.network_config().config().ethernet().address(), "192.168.0.5/24");
    EXPECT_EQ(configurator.ethernet_calls, 0);  // Get never applies
}

}  // namespace
