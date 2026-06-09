// WiFi Direct handler: credential mapping + teardown (U12, R23). Pure —
// fake IWifiManager + IDhcpServer + real SessionManager.

#include "app/control/services/handlers/wifi-direct.handler.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "app/control/ports/dhcp-server.hpp"
#include "app/control/ports/wifi-manager.hpp"
#include "app/session/ports/session-cleanup.hpp"
#include "app/session/services/session_manager/session-manager.hpp"
#include "bluetooth.pb.h"
#include "domain/network/models/wifi-direct-group.hpp"

namespace {

using sst::control::WifiDirectHandler;

class FakeWifi final : public sst::control::IWifiManager {
   public:
    auto StartP2pGroupOwner() -> std::optional<sst::network::WifiDirectGroup> override {
        ++starts;
        if (!ok) {
            return std::nullopt;
        }
        sst::network::WifiDirectGroup g;
        g.ssid = "DIRECT-xy";
        g.psk = "secretpass";
        g.group_interface = "p2p-wlan0-0";
        g.group_owner_ip = "192.168.49.1";
        g.role = "GO";
        return g;
    }
    auto Stop() -> void override { ++stops; }
    [[nodiscard]] auto State() const -> sst::control::WifiState override { return {}; }

    bool ok{true};
    int starts{0};
    int stops{0};
};

class FakeDhcp final : public sst::control::IDhcpServer {
   public:
    auto Start(const std::string& iface, const std::string& ip) -> bool override {
        started_iface = iface;
        started_ip = ip;
        ++starts;
        return ok;
    }
    auto Stop() -> void override { ++stops; }
    bool ok{true};
    int starts{0};
    int stops{0};
    std::string started_iface;
    std::string started_ip;
};

class FakeCleanup final : public sst::session::ISessionCleanup {
   public:
    auto FinalizeRecording() -> void override {}
    auto StopStreaming() -> void override {}
    auto TeardownWifiDirect() -> void override {}
};

auto StartCmd() -> sst_cam::Command {
    sst_cam::Command c;
    c.set_correlation_id("w");
    c.mutable_start_wifi_direct();
    return c;
}
auto StopCmd() -> sst_cam::Command {
    sst_cam::Command c;
    c.set_correlation_id("w");
    c.mutable_stop_wifi_direct();
    return c;
}

// R23: StartWifiDirect returns the camera-generated creds + GO IP + ports + role
// and advances the session to WifiReady; DHCP is started on the group interface.
TEST(WifiDirectHandlerTest, StartReportsGeneratedCredentials) {
    FakeCleanup cleanup;
    sst::session::SessionManager sm(cleanup);
    sm.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    WifiDirectHandler handler(sm, wifi, dhcp, /*preview*/ 8554, /*download*/ 8080);

    auto resp = handler.Handle(StartCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kWifiDirectGroup);
    const auto& g = resp.wifi_direct_group();
    EXPECT_EQ(g.ssid(), "DIRECT-xy");
    EXPECT_EQ(g.psk(), "secretpass");
    EXPECT_EQ(g.group_owner_ip(), "192.168.49.1");
    EXPECT_EQ(g.preview_port(), 8554U);
    EXPECT_EQ(g.download_port(), 8080U);
    EXPECT_EQ(g.role(), "GO");

    EXPECT_EQ(dhcp.started_iface, "p2p-wlan0-0");
    EXPECT_EQ(dhcp.started_ip, "192.168.49.1");
    EXPECT_EQ(sm.Phase(), sst::session::SessionPhase::kWifiReady);
}

// Group-owner formation failure -> ERROR, no DHCP.
TEST(WifiDirectHandlerTest, GroupFailureErrors) {
    FakeCleanup cleanup;
    sst::session::SessionManager sm(cleanup);
    sm.OnConnect();
    FakeWifi wifi;
    wifi.ok = false;
    FakeDhcp dhcp;
    WifiDirectHandler handler(sm, wifi, dhcp, 8554, 8080);

    auto resp = handler.Handle(StartCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_EQ(dhcp.starts, 0);
}

// StopWifiDirect tears down DHCP + the group and drops the session phase.
TEST(WifiDirectHandlerTest, StopTearsDownGroupAndDhcp) {
    FakeCleanup cleanup;
    sst::session::SessionManager sm(cleanup);
    sm.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    WifiDirectHandler handler(sm, wifi, dhcp, 8554, 8080);
    handler.Handle(StartCmd());

    auto resp = handler.Handle(StopCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(dhcp.stops, 1);
    EXPECT_EQ(wifi.stops, 1);
    EXPECT_EQ(sm.Phase(), sst::session::SessionPhase::kConnected);
}

}  // namespace
