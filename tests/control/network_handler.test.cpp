#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string>

#include "app/control/services/handlers/network.handler.hpp"
#include "app/network/ports/ethernet-controller.hpp"
#include "app/network/ports/uplink-configurator.hpp"
#include "app/network/ports/uplink-store.hpp"
#include "app/network/services/uplink-manager/uplink-manager.hpp"
#include "bluetooth.pb.h"
#include "domain/config/models/uplink-config.hpp"
#include "domain/network/models/ethernet-mode.hpp"

namespace {

// The fixed migration id the FakeArbiter mints, shared by the tests that drive
// the confirm/echo path so the value is named, not a bare literal.
constexpr sst::network::MigrationId kFakeMigrationId = 7;

// WiFi-STA configurator fake (Ethernet moved to IEthernetController below).
class FakeConfigurator final : public sst::network::IUplinkConfigurator {
   public:
    auto ApplyWifiSta(const sst::config::WifiStaUplink& /*cfg*/)
        -> sst::network::UplinkResult override {
        return {.ok = false, .detail = "unavailable"};
    }
    auto TeardownWifiSta() -> void override {}
    auto ProbeWifiSta() -> sst::network::UplinkResult override {
        return {.ok = false, .detail = "unavailable"};
    }
};

// Firmware-owned Ethernet fake: the apply outcome + the probed mode/address are
// parameterizable so a test can drive success, apply-failure, and — crucially —
// the REAL static-vs-DHCP reporting the handler now sources from here (R12/AE5).
class FakeEthernet final : public sst::network::IEthernetController {
   public:
    auto ApplyEthernet(const sst::config::EthernetUplink& cfg)
        -> sst::network::EthernetApplyResult override {
        ++apply_calls;
        last_apply_dhcp = cfg.dhcp.value_or(true);
        return apply_ok
                   ? sst::network::EthernetApplyResult{.ok = true, .detail = "10.10.1.30/24"}
                   : sst::network::EthernetApplyResult{.ok = false, .detail = "ip replace failed"};
    }
    auto ProbeEthernet() -> sst::network::EthernetProbe override {
        return {.mode = probe_mode, .address = probe_address, .carrier = carrier, .gateway = {}};
    }
    auto TeardownEthernet() -> void override { ++teardown_calls; }
    auto ProbeDhcpOffer() -> std::optional<std::string> override { return std::nullopt; }

    int apply_calls{0};
    int teardown_calls{0};
    bool apply_ok{true};
    bool last_apply_dhcp{true};
    // Default probe: a real STATIC link (the case that must never report DHCP).
    sst::network::EthernetMode probe_mode{sst::network::EthernetMode::kStatic};
    std::string probe_address{"10.10.1.30/24"};
    bool carrier{true};
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

// Fake connectivity arbiter (U4): the single LAN-path decision site. SelectLanPath
// mirrors precedence (Ethernet-on-subnet -> STA -> P2P) simply, so the handler's
// routing + response mapping is observable without the real radio/Ethernet.
class FakeArbiter final : public sst::network::IConnectivityArbiter {
   public:
    auto SelectLanPath(const sst::config::UplinkData& desired, const std::string& phone_subnet_cidr)
        -> sst::network::LanSelectionResult override {
        ++select_calls;
        last_phone_subnet = phone_subnet_cidr;
        if (!select_ok) {
            // A failed selection has already reverted to a legal kP2pOnly.
            mode = sst::network::ConnectivityMode::kP2pOnly;
            ssid.clear();
            current_lan_path = sst::network::LanPath::kP2p;
            return {.ok = false, .error = last_error, .path = sst::network::LanPath::kP2p};
        }
        if (desired.ethernet.enabled.value_or(false)) {
            current_lan_path = sst::network::LanPath::kEthernet;
            eth_endpoint = "10.10.1.30";
            eth_confirm_id = kFakeMigrationId;
            return {.ok = true,
                    .path = sst::network::LanPath::kEthernet,
                    .confirm_id = kFakeMigrationId,
                    .endpoint = eth_endpoint,
                    .credential = eth_credential};
        }
        if (desired.wifi.enabled.value_or(false)) {
            current_lan_path = sst::network::LanPath::kWifiSta;
            return {.ok = true,
                    .path = sst::network::LanPath::kWifiSta,
                    .confirm_id = kFakeMigrationId,
                    .endpoint = "10.0.0.9",
                    .credential = "cam:secret"};
        }
        current_lan_path = sst::network::LanPath::kP2p;
        return {.ok = true, .path = sst::network::LanPath::kP2p};
    }
    auto ConfirmSharedWifi(sst::network::MigrationId migration_id,
                           bool reachable) -> sst::network::ConnectivityMode override {
        ++confirm_calls;
        last_confirm_id = migration_id;
        last_reachable = reachable;
        return reachable ? sst::network::ConnectivityMode::kStaOnly
                         : sst::network::ConnectivityMode::kP2pOnly;
    }
    auto ConfirmLanPath(sst::network::MigrationId confirm_id, sst::network::LanPath path,
                        bool reachable) -> sst::network::LanPath override {
        ++confirm_lan_calls;
        last_lan_confirm_id = confirm_id;
        last_lan_confirm_path = path;
        last_reachable = reachable;
        current_lan_path = reachable ? path : sst::network::LanPath::kP2p;
        return current_lan_path;
    }
    auto RevertToP2p() -> void override { ++revert_calls; }
    [[nodiscard]] auto CurrentMode() const -> sst::network::ConnectivityMode override {
        return mode;
    }
    [[nodiscard]] auto CurrentLanPath() const -> sst::network::LanPath override {
        return current_lan_path;
    }
    [[nodiscard]] auto CurrentSsid() const -> std::string override { return ssid; }
    [[nodiscard]] auto LastError() const -> std::string override { return last_error; }
    [[nodiscard]] auto EthernetEndpoint() const -> std::string override { return eth_endpoint; }
    [[nodiscard]] auto EthernetCredential() const -> std::string override { return eth_credential; }
    [[nodiscard]] auto EthernetConfirmId() const -> sst::network::MigrationId override {
        return current_lan_path == sst::network::LanPath::kEthernet ? eth_confirm_id : 0;
    }
    // U8: the committed STA LAN handshake the handler surfaces on a plain Get.
    [[nodiscard]] auto PreviewEndpoint() const -> std::string override { return preview_endpoint; }
    [[nodiscard]] auto PreviewCredential() const -> std::string override {
        return preview_credential;
    }

    bool select_ok{true};
    int select_calls{0};
    int confirm_calls{0};
    int confirm_lan_calls{0};
    int revert_calls{0};
    sst::network::MigrationId last_confirm_id{0};
    sst::network::MigrationId last_lan_confirm_id{0};
    sst::network::LanPath last_lan_confirm_path{sst::network::LanPath::kNone};
    bool last_reachable{false};
    std::string last_phone_subnet;
    std::string eth_endpoint;
    std::string eth_credential;  // U6: the Ethernet-LAN handshake credential
    sst::network::MigrationId eth_confirm_id{0};
    // U8: the committed STA LAN handshake surfaced on a plain Get.
    std::string preview_endpoint;
    std::string preview_credential;
    // Settled state the handler reads back for the R3 connected-state / short-circuit.
    sst::network::ConnectivityMode mode{sst::network::ConnectivityMode::kP2pOnly};
    sst::network::LanPath current_lan_path{sst::network::LanPath::kP2p};
    std::string ssid;
    std::string last_error;
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
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet);

    const auto cases = handler.HandledCases();
    EXPECT_NE(std::find(cases.begin(), cases.end(), sst_cam::Command::kSetNetworkConfig),
              cases.end());
    EXPECT_NE(std::find(cases.begin(), cases.end(), sst_cam::Command::kGetNetworkConfig),
              cases.end());
}

TEST(NetworkHandlerTest, SetAppliesPersistsAndEchoesStatus) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet);

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
    // Routed through the arbiter's single decision site (no direct-apply bypass) +
    // persisted; the response reflects the winning LAN path.
    EXPECT_EQ(arbiter.select_calls, 1);
    EXPECT_EQ(store.persist_calls, 1);
    EXPECT_EQ(resp.network_config().lan_path(), sst_cam::LAN_PATH_ETHERNET);
}

// A failed LAN-path selection (an enabled interface could not come up) -> the
// response must NOT be a plain OK/green, and the bad config must NOT be persisted.
TEST(NetworkHandlerTest, SetWithFailedSelectionReportsErrorAndDoesNotPersist) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    arbiter.select_ok = false;  // the arbiter could not bring the interface up
    arbiter.last_error = "wired network setup failed";
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet);

    sst_cam::Command cmd;
    auto* eth = cmd.mutable_set_network_config()->mutable_config()->mutable_ethernet();
    eth->set_enabled(true);
    eth->set_dhcp(false);
    eth->set_address("10.10.1.30/24");
    eth->set_gateway("192.168.99.1");  // off-subnet -> apply fails inside the arbiter

    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(arbiter.select_calls, 1);                     // routed through the arbiter
    EXPECT_NE(resp.status(), sst_cam::ResponseStatus::OK);  // not a false green
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_EQ(store.persist_calls, 0);  // bad config not persisted as good
}

TEST(NetworkHandlerTest, GetReturnsInitialConfigWithoutApplying) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;

    sst::config::UplinkData initial;
    initial.ethernet.enabled = true;
    initial.ethernet.address = "192.168.0.5/24";

    sst::control::NetworkHandler handler(manager, store, initial, arbiter, ethernet);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kNetworkConfig);
    EXPECT_TRUE(resp.network_config().config().ethernet().enabled());
    EXPECT_EQ(resp.network_config().config().ethernet().address(), "192.168.0.5/24");
    EXPECT_TRUE(resp.network_config().ethernet_up());
    EXPECT_EQ(ethernet.apply_calls, 0);  // Get does not re-apply
}

// AE7: SetNetworkConfig arriving on an unencrypted/unbonded link is rejected —
// ERROR, the uplink is NOT applied, and nothing is persisted (the passphrase
// never crosses cleartext).
TEST(NetworkHandlerTest, SetOnInsecureLinkRejectedNotAppliedNotPersisted) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    FakeLinkSecurity insecure(/*secure=*/false);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet, &insecure);

    const auto resp = handler.Handle(MakeSetEthernetCommand());

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_FALSE(resp.error_message().empty());  // a clear reason for the app
    EXPECT_EQ(arbiter.select_calls, 0);          // arbiter never consulted
    EXPECT_EQ(store.persist_calls, 0);           // credentials not persisted
}

// AE7 complement: on an encrypted/bonded link the credential command proceeds
// exactly as before — applied and persisted.
TEST(NetworkHandlerTest, SetOnSecureLinkProceedsAppliesAndPersists) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet, &secure);

    const auto resp = handler.Handle(MakeSetEthernetCommand());

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(arbiter.select_calls, 1);  // routed through the arbiter
    EXPECT_EQ(store.persist_calls, 1);   // persisted
}

// GetNetworkConfig is not credential-bearing, so the link-security gate must not
// affect it even on an insecure link (reconnect/status UX preserved).
TEST(NetworkHandlerTest, GetUnaffectedByInsecureLink) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    FakeLinkSecurity insecure(/*secure=*/false);

    sst::config::UplinkData initial;
    initial.ethernet.enabled = true;
    initial.ethernet.address = "192.168.0.5/24";
    sst::control::NetworkHandler handler(manager, store, initial, arbiter, ethernet, &insecure);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kNetworkConfig);
    EXPECT_TRUE(resp.network_config().config().ethernet().enabled());
    EXPECT_EQ(resp.network_config().config().ethernet().address(), "192.168.0.5/24");
    EXPECT_EQ(ethernet.apply_calls, 0);  // Get never applies
}

// Enabling WiFi routes through the arbiter (never the manager directly, so INV2
// holds) and the reply carries the migration id / STA endpoint / credential.
TEST(NetworkHandlerTest, SetWithWifiBeginsMigrationAndReportsEndpoint) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet, &secure);

    sst_cam::Command cmd;
    auto* set = cmd.mutable_set_network_config();
    set->set_phone_subnet_cidr("10.10.1.0/24");
    auto* wifi = set->mutable_config()->mutable_wifi();
    wifi->set_enabled(true);
    wifi->set_ssid("PhoneNet");
    wifi->set_password("hunter2pw");

    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(arbiter.select_calls, 1);                    // wifi routed to the arbiter
    EXPECT_EQ(arbiter.last_phone_subnet, "10.10.1.0/24");  // phone-subnet hint plumbed
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kNetworkConfig);
    EXPECT_EQ(resp.network_config().migration_id(), kFakeMigrationId);
    EXPECT_EQ(resp.network_config().preview_endpoint(), "10.0.0.9");
    EXPECT_EQ(resp.network_config().preview_credential(), "cam:secret");
    EXPECT_EQ(resp.network_config().lan_path(), sst_cam::LAN_PATH_WIFI_STA);
}

// A shared-wifi migration that cannot start (arbiter returns nullopt) -> ERROR,
// nothing persisted.
TEST(NetworkHandlerTest, SetWithWifiMigrationFailureReportsError) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    arbiter.select_ok = false;
    arbiter.last_error = "couldn't join that Wi-Fi network";
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet, &secure);

    sst_cam::Command cmd;
    auto* wifi = cmd.mutable_set_network_config()->mutable_config()->mutable_wifi();
    wifi->set_enabled(true);
    wifi->set_ssid("PhoneNet");

    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_EQ(arbiter.select_calls, 1);
    EXPECT_EQ(store.persist_calls, 0);
}

// Ethernet enabled + wifi disabled routes through the single decision site (not a
// direct apply) and commits the Ethernet LAN path — no STA migration handshake.
TEST(NetworkHandlerTest, SetEthernetOnlyRoutesThroughArbiterNoMigration) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet);

    const auto resp = handler.Handle(MakeSetEthernetCommand());  // wifi disabled

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(arbiter.select_calls, 1);
    EXPECT_EQ(resp.network_config().migration_id(), 0U);  // no STA handshake
    EXPECT_EQ(resp.network_config().lan_path(), sst_cam::LAN_PATH_ETHERNET);
    EXPECT_EQ(resp.network_config().ethernet_confirm_id(), kFakeMigrationId);
    EXPECT_EQ(resp.network_config().ethernet_endpoint(), "10.10.1.30");
}

// U6 / R11: the Set that selects Ethernet surfaces the Ethernet-LAN handshake
// credential (proto field 14) sourced from the arbiter — alongside the endpoint +
// confirm id — so the app can identity-probe the Ethernet IP before ConfirmLanPath.
TEST(NetworkHandlerTest, SetEthernetSurfacesEthernetCredentialOnHandshake) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    arbiter.eth_credential = "cam:ethsecret";  // the hook-minted per-path credential
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet, &secure);

    const auto resp = handler.Handle(MakeSetEthernetCommand());  // wifi disabled

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(resp.network_config().lan_path(), sst_cam::LAN_PATH_ETHERNET);
    EXPECT_EQ(resp.network_config().ethernet_endpoint(), "10.10.1.30");
    EXPECT_EQ(resp.network_config().ethernet_confirm_id(), kFakeMigrationId);
    EXPECT_EQ(resp.network_config().ethernet_credential(), "cam:ethsecret");
}

// A handler whose arbiter is settled with Ethernet committed as the LAN path + a
// live per-path credential — the reconnect state a plain Get must surface (R8).
auto EthernetCommittedArbiter(FakeArbiter& arbiter) -> void {
    arbiter.current_lan_path = sst::network::LanPath::kEthernet;
    arbiter.eth_endpoint = "10.10.1.30";
    arbiter.eth_confirm_id = kFakeMigrationId;
    arbiter.eth_credential = "cam:ethsecret";
}

// U8 / R8 / AE5 / AE9: a plain Get with Ethernet committed surfaces lan_path, the
// real Ethernet mode + address, AND the endpoint + confirm id + credential over an
// encrypted/bonded link — so the app can identity-gate "connected" on reconnect
// WITHOUT replaying a migration.
TEST(NetworkHandlerTest, GetWithEthernetCommittedSurfacesCredentialOnSecureLink) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;  // default: real STATIC link at 10.10.1.30/24
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    EthernetCommittedArbiter(arbiter);
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet, &secure);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(resp.network_config().lan_path(), sst_cam::LAN_PATH_ETHERNET);
    // Real per-interface state (AE5): static mode + address from the live probe.
    EXPECT_TRUE(resp.network_config().ethernet_up());
    EXPECT_EQ(resp.network_config().ethernet_address(), "10.10.1.30/24");
    ASSERT_TRUE(resp.network_config().has_ethernet_dhcp());
    EXPECT_FALSE(resp.network_config().ethernet_dhcp());
    // Endpoint + confirm id + credential all present for the identity probe (R8).
    EXPECT_EQ(resp.network_config().ethernet_endpoint(), "10.10.1.30");
    EXPECT_EQ(resp.network_config().ethernet_confirm_id(), kFakeMigrationId);
    EXPECT_EQ(resp.network_config().ethernet_credential(), "cam:ethsecret");
    EXPECT_EQ(ethernet.apply_calls, 0);  // Get never applies
}

// U8 SECURITY (INV6): a plain Get over an UNENCRYPTED/unbonded link surfaces the
// endpoint + confirm id (non-secret, needed to address the probe) but NEVER the
// credential — its confidentiality rests entirely on the encrypted BLE link.
TEST(NetworkHandlerTest, GetWithEthernetCommittedHidesCredentialOnInsecureLink) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    EthernetCommittedArbiter(arbiter);
    FakeLinkSecurity insecure(/*secure=*/false);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet, &insecure);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(resp.network_config().lan_path(), sst_cam::LAN_PATH_ETHERNET);
    EXPECT_EQ(resp.network_config().ethernet_endpoint(), "10.10.1.30");  // addressing ok
    EXPECT_EQ(resp.network_config().ethernet_confirm_id(), kFakeMigrationId);
    EXPECT_TRUE(resp.network_config().ethernet_credential().empty());  // GATED off cleartext
}

// U8 / R8: a plain Get with WiFi-STA committed surfaces lan_path=kWifiSta, the
// current SSID, AND the STA preview endpoint + credential over a secure link — with
// NO in-flight migration (migration_id == 0) so the app re-probes without a Set.
TEST(NetworkHandlerTest, GetWithStaCommittedSurfacesPreviewHandshakeOnSecureLink) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    ethernet.probe_mode = sst::network::EthernetMode::kDown;  // Ethernet is not the LAN here
    ethernet.probe_address = "";
    ethernet.carrier = false;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    arbiter.mode = sst::network::ConnectivityMode::kStaOnly;
    arbiter.ssid = "PhoneNet";
    arbiter.current_lan_path = sst::network::LanPath::kWifiSta;
    arbiter.preview_endpoint = "10.10.1.142";
    arbiter.preview_credential = "cam:stasecret";
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet, &secure);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(resp.network_config().lan_path(), sst_cam::LAN_PATH_WIFI_STA);
    EXPECT_TRUE(resp.network_config().in_shared_wifi());
    EXPECT_EQ(resp.network_config().current_ssid(), "PhoneNet");
    EXPECT_EQ(resp.network_config().preview_endpoint(), "10.10.1.142");
    EXPECT_EQ(resp.network_config().preview_credential(), "cam:stasecret");
    EXPECT_EQ(resp.network_config().migration_id(), 0U);  // no migration in flight
}

// U8 SECURITY: the committed-STA credential is likewise gated off an insecure link.
TEST(NetworkHandlerTest, GetWithStaCommittedHidesPreviewCredentialOnInsecureLink) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    ethernet.probe_mode = sst::network::EthernetMode::kDown;
    ethernet.probe_address = "";
    ethernet.carrier = false;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    arbiter.mode = sst::network::ConnectivityMode::kStaOnly;
    arbiter.ssid = "PhoneNet";
    arbiter.current_lan_path = sst::network::LanPath::kWifiSta;
    arbiter.preview_endpoint = "10.10.1.142";
    arbiter.preview_credential = "cam:stasecret";
    FakeLinkSecurity insecure(/*secure=*/false);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet, &insecure);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.network_config().lan_path(), sst_cam::LAN_PATH_WIFI_STA);
    EXPECT_EQ(resp.network_config().preview_endpoint(), "10.10.1.142");  // addressing ok
    EXPECT_TRUE(resp.network_config().preview_credential().empty());     // GATED
}

// U8: a P2P-only committed path surfaces lan_path=kP2p and NO LAN credential /
// endpoint of either kind — there is nothing to hand out.
TEST(NetworkHandlerTest, GetWithP2pOnlySurfacesNoLanCredential) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    ethernet.probe_mode = sst::network::EthernetMode::kDown;  // no Ethernet reality to derive
    ethernet.probe_address = "";
    ethernet.carrier = false;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    arbiter.current_lan_path = sst::network::LanPath::kP2p;
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet, &secure);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.network_config().lan_path(), sst_cam::LAN_PATH_P2P);
    EXPECT_TRUE(resp.network_config().ethernet_credential().empty());
    EXPECT_TRUE(resp.network_config().ethernet_endpoint().empty());
    EXPECT_TRUE(resp.network_config().preview_credential().empty());
    EXPECT_TRUE(resp.network_config().preview_endpoint().empty());
}

// U8 boot-before-Set gap (R12): before any operator Set has engaged the arbiter (it
// still reports its kP2p boot default), a plain Get derives lan_path from the LIVE
// Ethernet probe so it reports REALITY — but surfaces NO credential (none is
// committed/minted yet; the app commits via a Set to obtain one).
TEST(NetworkHandlerTest, GetBeforeAnySetDerivesEthernetLanFromLiveProbe) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;  // default: real STATIC link up at 10.10.1.30/24
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;  // pristine: kP2p boot default, never engaged by a Set
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet, &secure);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.network_config().lan_path(), sst_cam::LAN_PATH_ETHERNET);  // reality
    EXPECT_TRUE(resp.network_config().ethernet_up());
    EXPECT_EQ(resp.network_config().ethernet_address(), "10.10.1.30/24");
    ASSERT_TRUE(resp.network_config().has_ethernet_dhcp());
    EXPECT_FALSE(resp.network_config().ethernet_dhcp());  // real static, never defaulted DHCP
    EXPECT_TRUE(resp.network_config().ethernet_credential().empty());  // nothing committed
    EXPECT_EQ(arbiter.select_calls, 0);                                // Get never selects
}

// U8 boot-before-Set gap: once a Set HAS engaged the arbiter and it committed kP2p
// (e.g. a different-subnet Ethernet uplink → LAN falls to P2P), a plain Get trusts
// the arbiter and does NOT derive kEthernet from the (up) uplink interface — the
// uplink is never mislabeled as the LAN path.
TEST(NetworkHandlerTest, GetAfterSetTrustsArbiterAndDoesNotDeriveUplinkAsLan) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;  // Ethernet interface is up (a different-subnet uplink)
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    // Model the arbiter having run a Set that committed P2P as the LAN (uplink case).
    arbiter.current_lan_path = sst::network::LanPath::kP2p;
    FakeLinkSecurity secure(/*secure=*/true);
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet, &secure);

    // Drive a Set first (P2P outcome) so arbiter_engaged_ is set in the handler.
    sst_cam::Command set;
    set.mutable_set_network_config();  // no interfaces enabled -> arbiter returns kP2p
    handler.Handle(set);

    sst_cam::Command get;
    get.mutable_get_network_config();
    const auto resp = handler.Handle(get);

    EXPECT_EQ(arbiter.select_calls, 1);
    EXPECT_TRUE(resp.network_config().ethernet_up());                    // uplink is up
    EXPECT_EQ(resp.network_config().lan_path(), sst_cam::LAN_PATH_P2P);  // NOT derived Ethernet
    EXPECT_TRUE(resp.network_config().ethernet_credential().empty());
}

// ConfirmPreviewReachable routes the app verdict to the arbiter and echoes the
// settled mode.
TEST(NetworkHandlerTest, ConfirmPreviewReachableRoutesToArbiter) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet);

    EXPECT_NE(std::find(handler.HandledCases().begin(), handler.HandledCases().end(),
                        sst_cam::Command::kConfirmPreviewReachable),
              handler.HandledCases().end());

    sst_cam::Command cmd;
    auto* confirm = cmd.mutable_confirm_preview_reachable();
    confirm->set_migration_id(kFakeMigrationId);
    confirm->set_reachable(true);

    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(arbiter.confirm_calls, 1);
    EXPECT_EQ(arbiter.last_confirm_id, kFakeMigrationId);
    EXPECT_TRUE(arbiter.last_reachable);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kConfirmPreviewReachable);
    EXPECT_EQ(resp.confirm_preview_reachable().migration_id(), kFakeMigrationId);
    EXPECT_TRUE(resp.confirm_preview_reachable().in_shared_wifi());
}

// A wifi network under test: SSID + passphrase bundled so the two strings can't
// be silently swapped at a call site (and so the helpers take one param, not two
// adjacent std::strings).
struct WifiCase {
    std::string ssid;
    std::string password;
};

// Build a SetNetworkConfig command enabling wifi for `net`.
auto MakeSetWifiCommand(const WifiCase& net) -> sst_cam::Command {
    sst_cam::Command cmd;
    auto* wifi = cmd.mutable_set_network_config()->mutable_config()->mutable_wifi();
    wifi->set_enabled(true);
    wifi->set_ssid(net.ssid);
    wifi->set_password(net.password);
    return cmd;
}

// A handler pre-seeded as "connected in S2 to `net`" — pair with an arbiter set to
// kStaOnly on the same SSID; the last-applied config carries the passphrase.
auto ConnectedInitial(const WifiCase& net) -> sst::config::UplinkData {
    sst::config::UplinkData initial;
    initial.wifi.enabled = true;
    initial.wifi.ssid = net.ssid;
    initial.wifi.passphrase = net.password;
    return initial;
}

// R3: re-tapping Connect on the network we are ALREADY on (same SSID + same
// passphrase) is a no-op — no arbiter call, no radio churn, NOT re-persisted — and
// the reply reports the connected state (in_shared_wifi + current_ssid), not "in
// progress".
TEST(NetworkHandlerTest, SetSameNetworkWhileConnectedIsNoOpAndReportsConnected) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    arbiter.mode = sst::network::ConnectivityMode::kStaOnly;
    arbiter.ssid = "PhoneNet";
    sst::control::NetworkHandler handler(
        manager, store, ConnectedInitial({"PhoneNet", "hunter2pw"}), arbiter, ethernet);

    const auto resp = handler.Handle(MakeSetWifiCommand({"PhoneNet", "hunter2pw"}));

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(arbiter.select_calls, 0);  // no migration attempted
    EXPECT_EQ(arbiter.revert_calls, 0);  // no revert
    EXPECT_EQ(store.persist_calls, 0);   // creds not re-persisted (already validated)
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kNetworkConfig);
    EXPECT_TRUE(resp.network_config().in_shared_wifi());
    EXPECT_EQ(resp.network_config().current_ssid(), "PhoneNet");
    EXPECT_EQ(resp.network_config().migration_id(), 0U);  // no handshake
}

// R3: same SSID but a DIFFERENT passphrase is NOT a no-op — it re-migrates so the
// arbiter validates the new creds, and only then are they persisted (guards
// saved-but-never-applied creds poisoning the next boot).
TEST(NetworkHandlerTest, SetSameSsidDifferentPasswordReMigrates) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    arbiter.mode = sst::network::ConnectivityMode::kStaOnly;
    arbiter.ssid = "PhoneNet";
    sst::control::NetworkHandler handler(manager, store, ConnectedInitial({"PhoneNet", "oldpw"}),
                                         arbiter, ethernet);

    const auto resp = handler.Handle(MakeSetWifiCommand({"PhoneNet", "newpassword"}));

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(arbiter.select_calls, 1);  // re-migrated, not short-circuited
    EXPECT_EQ(store.persist_calls, 1);   // persisted only after the arbiter validated
    EXPECT_EQ(resp.network_config().migration_id(), kFakeMigrationId);
}

// R3: a change-networks to a NEW SSID that FAILS reports ERROR, does not persist,
// rolls current_ back (echoes the previous SSID, not the failed one), and shows the
// arbiter's reverted state (in_shared_wifi=false) — the destructive-failure path.
TEST(NetworkHandlerTest, SetChangeNetworksFailureRollsBackAndReportsError) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    arbiter.mode = sst::network::ConnectivityMode::kStaOnly;
    arbiter.ssid = "OldNet";
    arbiter.select_ok = false;  // the new join fails; arbiter reverts to P2P
    arbiter.last_error = "failed to join wifi (check the password)";
    sst::control::NetworkHandler handler(manager, store, ConnectedInitial({"OldNet", "oldpw"}),
                                         arbiter, ethernet);

    const auto resp = handler.Handle(MakeSetWifiCommand({"NewNet", "newpw"}));

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_FALSE(resp.error_message().empty());
    EXPECT_EQ(arbiter.select_calls, 1);
    EXPECT_EQ(store.persist_calls, 0);  // failed config not persisted
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kNetworkConfig);
    EXPECT_FALSE(resp.network_config().in_shared_wifi());               // reverted to P2P
    EXPECT_EQ(resp.network_config().config().wifi().ssid(), "OldNet");  // current_ rolled back
}

// R3: a plain Get while connected reports the connected state deterministically so
// the app can render "connected to <ssid>" on reconnect without the handshake.
TEST(NetworkHandlerTest, GetWhileConnectedReportsInSharedWifiAndSsid) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    arbiter.mode = sst::network::ConnectivityMode::kStaOnly;
    arbiter.ssid = "PhoneNet";
    sst::control::NetworkHandler handler(manager, store, ConnectedInitial({"PhoneNet", "pw"}),
                                         arbiter, ethernet);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_TRUE(resp.network_config().in_shared_wifi());
    EXPECT_EQ(resp.network_config().current_ssid(), "PhoneNet");
    EXPECT_EQ(ethernet.apply_calls, 0);  // Get never applies
}

// AE5 / R12: GetNetworkConfig sources the Ethernet mode from the REAL probe, so a
// statically configured link reports ethernet_dhcp = FALSE (present) — never the
// old default-DHCP placeholder ("DHCP active" for a static link).
TEST(NetworkHandlerTest, GetReportsRealStaticEthernetNeverFalseDhcp) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    ethernet.probe_mode = sst::network::EthernetMode::kStatic;
    ethernet.probe_address = "10.10.1.30/24";
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kNetworkConfig);
    EXPECT_TRUE(resp.network_config().ethernet_up());
    EXPECT_EQ(resp.network_config().ethernet_address(), "10.10.1.30/24");
    // ethernet_dhcp is PRESENT and FALSE — the DHCP-when-static bug is gone.
    ASSERT_TRUE(resp.network_config().has_ethernet_dhcp());
    EXPECT_FALSE(resp.network_config().ethernet_dhcp());
}

// R12: a DHCP link reports ethernet_dhcp = TRUE (present).
TEST(NetworkHandlerTest, GetReportsRealDhcpEthernet) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    ethernet.probe_mode = sst::network::EthernetMode::kDhcp;
    ethernet.probe_address = "192.168.1.42/24";
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    ASSERT_TRUE(resp.network_config().has_ethernet_dhcp());
    EXPECT_TRUE(resp.network_config().ethernet_dhcp());
    EXPECT_TRUE(resp.network_config().ethernet_up());
}

// R12: a down link leaves ethernet_dhcp ABSENT (unknown, not a defaulted DHCP) and
// reports ethernet_up=false with no address.
TEST(NetworkHandlerTest, GetReportsDownEthernetLeavesDhcpUnset) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    ethernet.probe_mode = sst::network::EthernetMode::kDown;
    ethernet.probe_address = "";
    ethernet.carrier = false;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet);

    sst_cam::Command cmd;
    cmd.mutable_get_network_config();
    const auto resp = handler.Handle(cmd);

    EXPECT_FALSE(resp.network_config().ethernet_up());
    EXPECT_TRUE(resp.network_config().ethernet_address().empty());
    EXPECT_FALSE(resp.network_config().has_ethernet_dhcp());
}

// R11/R19: ConfirmLanPathCommand is handled — routed to the arbiter's ConfirmLanPath
// (confirm id + path) and the committed path is echoed back.
TEST(NetworkHandlerTest, ConfirmLanPathRoutesToArbiter) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);
    FakeStore store;
    FakeArbiter arbiter;
    sst::control::NetworkHandler handler(manager, store, {}, arbiter, ethernet);

    EXPECT_NE(std::find(handler.HandledCases().begin(), handler.HandledCases().end(),
                        sst_cam::Command::kConfirmLanPath),
              handler.HandledCases().end());

    sst_cam::Command cmd;
    auto* confirm = cmd.mutable_confirm_lan_path();
    confirm->set_confirm_id(kFakeMigrationId);
    confirm->set_path(sst_cam::LAN_PATH_ETHERNET);
    confirm->set_reachable(true);

    const auto resp = handler.Handle(cmd);

    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(arbiter.confirm_lan_calls, 1);
    EXPECT_EQ(arbiter.last_lan_confirm_id, kFakeMigrationId);
    EXPECT_EQ(arbiter.last_lan_confirm_path, sst::network::LanPath::kEthernet);
    EXPECT_TRUE(arbiter.last_reachable);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kConfirmLanPath);
    EXPECT_EQ(resp.confirm_lan_path().confirm_id(), kFakeMigrationId);
    EXPECT_EQ(resp.confirm_lan_path().committed_path(), sst_cam::LAN_PATH_ETHERNET);
    EXPECT_TRUE(resp.confirm_lan_path().committed());
}

}  // namespace
