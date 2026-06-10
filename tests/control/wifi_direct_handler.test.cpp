// WiFi Direct handler: credential mapping + teardown (U12, R23). Pure —
// fake IWifiManager + IDhcpServer + real SessionManager.

#include "app/control/services/handlers/wifi-direct.handler.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include <cstdint>
#include <vector>

#include "app/control/ports/dhcp-server.hpp"
#include "app/control/ports/wifi-manager.hpp"
#include "app/session/ports/session-cleanup.hpp"
#include "app/session/services/session_manager/session-manager.hpp"
#include "app/streaming/ports/streaming-service.hpp"
#include "bluetooth.pb.h"
#include "domain/network/models/wifi-direct-group.hpp"
#include "domain/streaming/models/app-stream-config.hpp"

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

// Records the AppStreamConfig the handler starts the preview with, and lets a
// test force a start failure to exercise the preview-degraded path.
class FakeStreaming final : public sst::streaming::IStreamingService {
   public:
    auto StartAppStream(const sst::streaming::AppStreamConfig& config) -> bool override {
        ++app_starts;
        last_config = config;
        return app_start_ok;
    }
    auto StopAppStream() -> bool override {
        ++app_stops;
        return true;
    }
    [[nodiscard]] auto IsAppStreamRunning() const -> bool override { return app_starts > app_stops; }
    auto StartPlatformStream(const sst::streaming::PlatformStreamConfig& /*config*/)
        -> bool override {
        return true;
    }
    auto StopPlatformStream(std::int64_t /*stream_id*/) -> bool override { return true; }
    [[nodiscard]] auto ListActivePlatformStreams() const
        -> std::vector<sst::streaming::ActivePlatformStream> override {
        return {};
    }

    bool app_start_ok{true};
    int app_starts{0};
    int app_stops{0};
    sst::streaming::AppStreamConfig last_config;
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
    FakeStreaming streaming;
    WifiDirectHandler handler(sm, wifi, dhcp, streaming, /*preview*/ 8554, /*download*/ 8080);

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

    // RTSP preview started, bound to the group-owner IP + preview port.
    EXPECT_EQ(streaming.app_starts, 1);
    EXPECT_EQ(streaming.last_config.address, "192.168.49.1");
    EXPECT_EQ(streaming.last_config.port, 8554U);
}

// Group-owner formation failure -> ERROR, no DHCP.
TEST(WifiDirectHandlerTest, GroupFailureErrors) {
    FakeCleanup cleanup;
    sst::session::SessionManager sm(cleanup);
    sm.OnConnect();
    FakeWifi wifi;
    wifi.ok = false;
    FakeDhcp dhcp;
    FakeStreaming streaming;
    WifiDirectHandler handler(sm, wifi, dhcp, streaming, 8554, 8080);

    auto resp = handler.Handle(StartCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_EQ(dhcp.starts, 0);
    EXPECT_EQ(streaming.app_starts, 0);  // no preview when the group never formed
}

// If the session SM rejects WifiReady (e.g. no central connected), the handler
// rolls the group + DHCP back and reports ERROR instead of handing out creds for
// a group the session can't use.
TEST(WifiDirectHandlerTest, StartRollsBackWhenSessionRejects) {
    FakeCleanup cleanup;
    sst::session::SessionManager sm(cleanup);
    // NOTE: no sm.OnConnect() — phase is Idle, so OnWifiReady() returns false.
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeStreaming streaming;
    WifiDirectHandler handler(sm, wifi, dhcp, streaming, 8554, 8080);

    auto resp = handler.Handle(StartCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_NE(resp.payload_case(), sst_cam::CommandResponse::kWifiDirectGroup);
    EXPECT_EQ(wifi.starts, 1);
    EXPECT_EQ(wifi.stops, 1);   // group rolled back
    EXPECT_EQ(dhcp.starts, 1);
    EXPECT_EQ(dhcp.stops, 1);   // DHCP rolled back
    EXPECT_EQ(streaming.app_starts, 0);  // preview never started (rolled back before it)
    EXPECT_EQ(sm.Phase(), sst::session::SessionPhase::kIdle);
}

// StopWifiDirect tears down DHCP + the group and drops the session phase.
TEST(WifiDirectHandlerTest, StopTearsDownGroupAndDhcp) {
    FakeCleanup cleanup;
    sst::session::SessionManager sm(cleanup);
    sm.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeStreaming streaming;
    WifiDirectHandler handler(sm, wifi, dhcp, streaming, 8554, 8080);
    handler.Handle(StartCmd());

    auto resp = handler.Handle(StopCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(dhcp.stops, 1);
    EXPECT_EQ(wifi.stops, 1);
    EXPECT_EQ(sm.Phase(), sst::session::SessionPhase::kConnected);
}

// A preview start failure is degraded-but-not-fatal: the handler still returns
// OK with the group credentials (the group + download path still work).
TEST(WifiDirectHandlerTest, PreviewFailureIsDegradedNotFatal) {
    FakeCleanup cleanup;
    sst::session::SessionManager sm(cleanup);
    sm.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeStreaming streaming;
    streaming.app_start_ok = false;  // RTSP preview fails to start
    WifiDirectHandler handler(sm, wifi, dhcp, streaming, 8554, 8080);

    auto resp = handler.Handle(StartCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kWifiDirectGroup);
    EXPECT_EQ(streaming.app_starts, 1);  // attempted
    // Group is not rolled back on a preview failure.
    EXPECT_EQ(wifi.stops, 0);
    EXPECT_EQ(dhcp.stops, 0);
    EXPECT_EQ(sm.Phase(), sst::session::SessionPhase::kWifiReady);
}

}  // namespace
