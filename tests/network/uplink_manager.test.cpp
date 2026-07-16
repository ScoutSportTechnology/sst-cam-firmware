#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "app/network/ports/ethernet-controller.hpp"
#include "app/network/ports/uplink-configurator.hpp"
#include "app/network/services/uplink-manager/uplink-manager.hpp"
#include "domain/config/models/uplink-config.hpp"
#include "domain/network/models/ethernet-mode.hpp"

namespace {

// Records which WiFi-STA calls the manager drove + returns canned results, so the
// orchestration (enabled→apply, disabled→skip) is tested without wpa_supplicant.
// Ethernet is no longer owned here — it lives on IEthernetController below.
class FakeConfigurator final : public sst::network::IUplinkConfigurator {
   public:
    auto ApplyWifiSta(const sst::config::WifiStaUplink& /*cfg*/)
        -> sst::network::UplinkResult override {
        ++wifi_calls;
        return {.ok = false, .detail = "unavailable"};
    }
    auto TeardownWifiSta() -> void override {}
    auto ProbeWifiSta() -> sst::network::UplinkResult override {
        return {.ok = false, .detail = "unavailable"};
    }

    int wifi_calls{0};
};

// Firmware-owned Ethernet fake: records apply/probe, echoes the requested mode.
class FakeEthernet final : public sst::network::IEthernetController {
   public:
    auto ApplyEthernet(const sst::config::EthernetUplink& cfg)
        -> sst::network::EthernetApplyResult override {
        ++ethernet_calls;
        last_dhcp = cfg.dhcp.value_or(true);
        return {.ok = true, .detail = "10.10.1.30/24"};
    }
    auto ProbeEthernet() -> sst::network::EthernetProbe override {
        ++ethernet_probes;
        return {.mode = sst::network::EthernetMode::kStatic,
                .address = "10.10.1.30/24",
                .carrier = true,
                .gateway = {}};
    }
    auto ProbeDhcpOffer() -> std::optional<std::string> override { return std::nullopt; }

    int ethernet_calls{0};
    int ethernet_probes{0};
    bool last_dhcp{true};
};

TEST(UplinkManagerTest, AppliesEnabledEthernetSkipsDisabledWifi) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);

    sst::config::UplinkData cfg;
    cfg.ethernet.enabled = true;
    cfg.ethernet.dhcp = false;
    cfg.wifi.enabled = false;

    const auto status = manager.Apply(cfg);

    EXPECT_EQ(ethernet.ethernet_calls, 1);
    EXPECT_EQ(configurator.wifi_calls, 0);  // disabled → skipped
    EXPECT_FALSE(ethernet.last_dhcp);
    EXPECT_TRUE(status.ethernet_enabled);
    EXPECT_TRUE(status.ethernet.ok);
    EXPECT_EQ(status.ethernet.detail, "10.10.1.30/24");
    EXPECT_FALSE(status.wifi_enabled);
}

TEST(UplinkManagerTest, BothDisabledDrivesNothing) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);

    sst::config::UplinkData cfg;  // both default-disabled (nullopt → false)

    const auto status = manager.Apply(cfg);

    EXPECT_EQ(ethernet.ethernet_calls, 0);
    EXPECT_EQ(configurator.wifi_calls, 0);
    EXPECT_FALSE(status.ethernet_enabled);
    EXPECT_FALSE(status.wifi_enabled);
}

// AE5 / R6: with both configured but wifi deactivated, only ethernet is driven.
TEST(UplinkManagerTest, WifiEnabledIsDrivenButReportsGatedUnavailable) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);

    sst::config::UplinkData cfg;
    cfg.ethernet.enabled = true;
    cfg.wifi.enabled = true;

    const auto status = manager.Apply(cfg);

    EXPECT_EQ(ethernet.ethernet_calls, 1);
    EXPECT_EQ(configurator.wifi_calls, 1);
    EXPECT_TRUE(status.wifi_enabled);
    EXPECT_FALSE(status.wifi.ok);  // gated on single-radio hardware
}

// Fix B: QueryStatus probes the LIVE interface state (reality), independent of
// any apply — so GetNetworkConfig shows the camera's actual ethernet/wifi.
TEST(UplinkManagerTest, QueryStatusProbesLiveState) {
    FakeConfigurator configurator;
    FakeEthernet ethernet;
    sst::network::UplinkManager manager(configurator, ethernet);

    const auto status = manager.QueryStatus();

    EXPECT_EQ(ethernet.ethernet_probes, 1);
    EXPECT_EQ(ethernet.ethernet_calls, 0);  // probe, not apply
    EXPECT_TRUE(status.ethernet.ok);        // kStatic → up
    EXPECT_EQ(status.ethernet.detail, "10.10.1.30/24");
    EXPECT_FALSE(status.wifi.ok);
}

}  // namespace
