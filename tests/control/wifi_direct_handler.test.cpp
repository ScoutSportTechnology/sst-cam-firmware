// WiFi Direct handler: credential mapping + teardown (U12, R23). Pure —
// fake IWifiManager + IDhcpServer + real SessionManager.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "app/control/ports/dhcp-server.hpp"
#include "app/control/ports/network-configurator.hpp"
#include "app/control/ports/wifi-manager.hpp"
#include "app/control/services/handlers/wifi-direct.handler.hpp"
#include "app/session/ports/session-cleanup.hpp"
#include "app/session/services/session_manager/session-manager.hpp"
#include "app/streaming/ports/streaming-service.hpp"
#include "bluetooth.pb.h"
#include "domain/network/models/wifi-direct-group.hpp"
#include "domain/streaming/models/app-stream-config.hpp"

namespace {

using sst::control::WifiDirectHandler;

// RTSP preview + HTTP download ports the handler advertises in the group reply.
constexpr std::uint32_t kPreviewPort = 8554;
constexpr std::uint32_t kDownloadPort = 8080;

class FakeWifi final : public sst::control::IWifiManager {
   public:
    auto StartP2pGroupOwner() -> std::optional<sst::network::WifiDirectGroup> override {
        ++starts;
        if (!ok) {
            return std::nullopt;
        }
        sst::network::WifiDirectGroup group;
        group.ssid = "DIRECT-xy";
        group.psk = "secretpass";
        group.group_interface = "p2p-wlan0-0";
        group.group_owner_ip = "192.168.49.1";
        group.role = "GO";
        return group;
    }
    auto Stop() -> void override { ++stops; }
    [[nodiscard]] auto State() const -> sst::control::WifiState override { return {}; }

    bool ok{true};
    int starts{0};
    int stops{0};
};

class FakeDhcp final : public sst::control::IDhcpServer {
   public:
    auto Start(const std::string& iface, const std::string& addr) -> bool override {
        started_iface = iface;
        started_ip = addr;
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

// Records the GO-IP assignment + clear; `ok` forces an assignment failure.
class FakeNetworkConfigurator final : public sst::control::INetworkConfigurator {
   public:
    auto AssignGroupOwnerAddress(const std::string& iface,
                                 const std::string& cidr) -> bool override {
        ++assigns;
        assigned_iface = iface;
        assigned_cidr = cidr;
        return ok;
    }
    auto Clear(const std::string& iface) -> void override {
        ++clears;
        cleared_iface = iface;
    }
    bool ok{true};
    int assigns{0};
    int clears{0};
    std::string assigned_iface;
    std::string assigned_cidr;
    std::string cleared_iface;
};

class FakeCleanup final : public sst::session::ISessionCleanup {
   public:
    auto FinalizeRecording() -> bool override { return true; }
    auto StopStreaming() -> void override {}
    auto TeardownWifiDirect() -> void override {}
    auto ResetSelections() -> void override {}
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
    [[nodiscard]] auto IsAppStreamRunning() const -> bool override {
        return app_starts > app_stops;
    }
    auto RebindAppStream(const sst::streaming::AppStreamConfig& config) -> bool override {
        ++app_rebinds;
        last_config = config;
        return app_start_ok;
    }
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
    int app_rebinds{0};
    sst::streaming::AppStreamConfig last_config;
};

auto StartCmd() -> sst_cam::Command {
    sst_cam::Command cmd;
    cmd.set_correlation_id("w");
    cmd.mutable_start_wifi_direct();
    return cmd;
}
auto StopCmd() -> sst_cam::Command {
    sst_cam::Command cmd;
    cmd.set_correlation_id("w");
    cmd.mutable_stop_wifi_direct();
    return cmd;
}

// R23: StartWifiDirect returns the camera-generated creds + GO IP + ports + role
// and advances the session to WifiReady; DHCP is started on the group interface.
TEST(WifiDirectHandlerTest, StartReportsGeneratedCredentials) {
    FakeCleanup cleanup;
    sst::session::SessionManager manager(cleanup);
    manager.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    FakeStreaming streaming;
    WifiDirectHandler handler(manager, wifi, netcfg, dhcp, streaming,
                              sst::control::PreviewPort{kPreviewPort},
                              sst::control::DownloadPort{kDownloadPort});

    auto resp = handler.Handle(StartCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kWifiDirectGroup);
    const auto& group = resp.wifi_direct_group();
    EXPECT_EQ(group.ssid(), "DIRECT-xy");
    EXPECT_EQ(group.psk(), "secretpass");
    EXPECT_EQ(group.group_owner_ip(), "192.168.49.1");
    EXPECT_EQ(group.preview_port(), kPreviewPort);
    EXPECT_EQ(group.download_port(), kDownloadPort);
    EXPECT_EQ(group.role(), "GO");

    // GO IP assigned to the group interface (with /24) before DHCP/RTSP.
    EXPECT_EQ(netcfg.assigns, 1);
    EXPECT_EQ(netcfg.assigned_iface, "p2p-wlan0-0");
    EXPECT_EQ(netcfg.assigned_cidr, "192.168.49.1/24");

    EXPECT_EQ(dhcp.started_iface, "p2p-wlan0-0");
    EXPECT_EQ(dhcp.started_ip, "192.168.49.1");
    EXPECT_EQ(manager.Phase(), sst::session::SessionPhase::kWifiReady);

    // RTSP preview started, bound to the group-owner IP + preview port.
    EXPECT_EQ(streaming.app_starts, 1);
    EXPECT_EQ(streaming.last_config.address, "192.168.49.1");
    EXPECT_EQ(streaming.last_config.port, kPreviewPort);
}

// Group-owner formation failure -> ERROR, no DHCP.
TEST(WifiDirectHandlerTest, UnhealthyCamerasAllowWifiButDelayPreviewUntilHealthy) {
    FakeCleanup cleanup;
    sst::session::SessionManager manager(cleanup);
    manager.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    FakeStreaming streaming;
    sst_cam::CameraHealth health = sst_cam::CAMERA_HEALTH_RECOVERING;
    WifiDirectHandler handler(
        manager, wifi, netcfg, dhcp, streaming, sst::control::PreviewPort{kPreviewPort},
        sst::control::DownloadPort{kDownloadPort}, [&](std::uint32_t) { return health; });
    EXPECT_EQ(handler.Handle(StartCmd()).status(), sst_cam::OK);
    EXPECT_EQ(streaming.app_starts, 0);
    EXPECT_EQ(wifi.starts, 1);
    health = sst_cam::CAMERA_HEALTH_OK;
    EXPECT_EQ(handler.Handle(StartCmd()).status(), sst_cam::OK);
    EXPECT_EQ(wifi.starts, 1);
    EXPECT_EQ(streaming.app_starts, 1);
    EXPECT_EQ(streaming.last_config.address, "192.168.49.1");
    EXPECT_EQ(streaming.last_config.port, kPreviewPort);
}

TEST(WifiDirectHandlerTest, RejoinReusesGroupWithoutReforming) {
    FakeCleanup cleanup;
    sst::session::SessionManager manager(cleanup);
    manager.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    FakeStreaming streaming;
    WifiDirectHandler handler(manager, wifi, netcfg, dhcp, streaming,
                              sst::control::PreviewPort{kPreviewPort},
                              sst::control::DownloadPort{kDownloadPort});
    ASSERT_EQ(handler.Handle(StartCmd()).status(), sst_cam::OK);
    manager.OnDisconnect();
    ASSERT_TRUE(manager.OnConnect());
    const auto response = handler.Handle(StartCmd());
    EXPECT_EQ(response.status(), sst_cam::OK);
    EXPECT_EQ(response.wifi_direct_group().ssid(), "DIRECT-xy");
    EXPECT_EQ(wifi.starts, 1);
    EXPECT_EQ(dhcp.starts, 1);
    EXPECT_EQ(streaming.app_starts, 1);
}

TEST(WifiDirectHandlerTest, StopCannotDropActiveRecordingGroup) {
    FakeCleanup cleanup;
    sst::session::SessionManager manager(cleanup);
    manager.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    FakeStreaming streaming;
    WifiDirectHandler handler(manager, wifi, netcfg, dhcp, streaming,
                              sst::control::PreviewPort{kPreviewPort},
                              sst::control::DownloadPort{kDownloadPort});
    ASSERT_EQ(handler.Handle(StartCmd()).status(), sst_cam::OK);
    sst::session::SessionConfig cfg;
    cfg.match_uuid = "match";
    ASSERT_TRUE(manager.ApplySessionConfig(cfg));
    ASSERT_TRUE(manager.OnOverlayConfigured());
    ASSERT_TRUE(manager.OnRecordingStart());
    EXPECT_EQ(handler.Handle(StopCmd()).status(), sst_cam::ERROR);
    EXPECT_EQ(wifi.stops, 0);
    EXPECT_EQ(manager.Phase(), sst::session::SessionPhase::kRecording);
}

TEST(WifiDirectHandlerTest, GroupFailureErrors) {
    FakeCleanup cleanup;
    sst::session::SessionManager manager(cleanup);
    manager.OnConnect();
    FakeWifi wifi;
    wifi.ok = false;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    FakeStreaming streaming;
    WifiDirectHandler handler(manager, wifi, netcfg, dhcp, streaming,
                              sst::control::PreviewPort{kPreviewPort},
                              sst::control::DownloadPort{kDownloadPort});

    auto resp = handler.Handle(StartCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_EQ(dhcp.starts, 0);
    EXPECT_EQ(streaming.app_starts, 0);  // no preview when the group never formed
}

// IP-assignment failure -> ERROR, group rolled back, and crucially DHCP/RTSP are
// never reached (proving assignment runs before them).
TEST(WifiDirectHandlerTest, IpAssignFailureRollsBackBeforeDhcp) {
    FakeCleanup cleanup;
    sst::session::SessionManager manager(cleanup);
    manager.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    netcfg.ok = false;  // GO-IP assignment fails
    FakeStreaming streaming;
    WifiDirectHandler handler(manager, wifi, netcfg, dhcp, streaming,
                              sst::control::PreviewPort{kPreviewPort},
                              sst::control::DownloadPort{kDownloadPort});

    auto resp = handler.Handle(StartCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_EQ(netcfg.assigns, 1);
    EXPECT_EQ(wifi.stops, 1);            // group rolled back
    EXPECT_EQ(dhcp.starts, 0);           // never reached
    EXPECT_EQ(streaming.app_starts, 0);  // never reached
}

// If the session SM rejects WifiReady (e.g. no central connected), the handler
// rolls the group + DHCP back and reports ERROR instead of handing out creds for
// a group the session can't use.
TEST(WifiDirectHandlerTest, StartRollsBackWhenSessionRejects) {
    FakeCleanup cleanup;
    sst::session::SessionManager manager(cleanup);
    // NOTE: no manager.OnConnect() — phase is Idle, so OnWifiReady() returns false.
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    FakeStreaming streaming;
    WifiDirectHandler handler(manager, wifi, netcfg, dhcp, streaming,
                              sst::control::PreviewPort{kPreviewPort},
                              sst::control::DownloadPort{kDownloadPort});

    auto resp = handler.Handle(StartCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_NE(resp.payload_case(), sst_cam::CommandResponse::kWifiDirectGroup);
    EXPECT_EQ(wifi.starts, 1);
    EXPECT_EQ(wifi.stops, 1);  // group rolled back
    EXPECT_EQ(dhcp.starts, 1);
    EXPECT_EQ(dhcp.stops, 1);            // DHCP rolled back
    EXPECT_EQ(streaming.app_starts, 0);  // preview never started (rolled back before it)
    EXPECT_EQ(manager.Phase(), sst::session::SessionPhase::kIdle);
}

// StopWifiDirect tears down DHCP + the group and drops the session phase.
TEST(WifiDirectHandlerTest, StopTearsDownGroupAndDhcp) {
    FakeCleanup cleanup;
    sst::session::SessionManager manager(cleanup);
    manager.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    FakeStreaming streaming;
    WifiDirectHandler handler(manager, wifi, netcfg, dhcp, streaming,
                              sst::control::PreviewPort{kPreviewPort},
                              sst::control::DownloadPort{kDownloadPort});
    handler.Handle(StartCmd());

    auto resp = handler.Handle(StopCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(dhcp.stops, 1);
    EXPECT_EQ(netcfg.clears, 1);  // GO address flushed on teardown
    EXPECT_EQ(wifi.stops, 1);
    EXPECT_EQ(manager.Phase(), sst::session::SessionPhase::kConnected);
}

// A preview start failure is degraded-but-not-fatal: the handler still returns
// OK with the group credentials (the group + download path still work).
TEST(WifiDirectHandlerTest, PreviewFailureIsDegradedNotFatal) {
    FakeCleanup cleanup;
    sst::session::SessionManager manager(cleanup);
    manager.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    FakeStreaming streaming;
    streaming.app_start_ok = false;  // RTSP preview fails to start
    WifiDirectHandler handler(manager, wifi, netcfg, dhcp, streaming,
                              sst::control::PreviewPort{kPreviewPort},
                              sst::control::DownloadPort{kDownloadPort});

    auto resp = handler.Handle(StartCmd());
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kWifiDirectGroup);
    EXPECT_EQ(streaming.app_starts, 1);  // attempted
    // Group is not rolled back on a preview failure.
    EXPECT_EQ(wifi.stops, 0);
    EXPECT_EQ(dhcp.stops, 0);
    EXPECT_EQ(manager.Phase(), sst::session::SessionPhase::kWifiReady);
}

// U6 / R6, R11: OnEthernetLanSelected mints a per-session RTSP credential (the SAME
// generator as the STA migration) and rebinds the authenticated preview onto the
// Ethernet IP — the arbiter-driven preview transport for the Ethernet LAN path. No
// WiFi-Direct group is formed/torn down (Ethernet is orthogonal to the radio).
TEST(WifiDirectHandlerTest, OnEthernetLanSelectedMintsCredentialAndRebindsOntoEthernetIp) {
    FakeCleanup cleanup;
    sst::session::SessionManager manager(cleanup);
    manager.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    FakeStreaming streaming;
    WifiDirectHandler handler(manager, wifi, netcfg, dhcp, streaming,
                              sst::control::PreviewPort{kPreviewPort},
                              sst::control::DownloadPort{kDownloadPort});

    const std::string credential = handler.OnEthernetLanSelected("10.10.1.30");

    // A real per-session credential (same "cam:<hex>" shape the STA path mints).
    EXPECT_FALSE(credential.empty());
    EXPECT_EQ(credential.rfind("cam:", 0), 0U);
    // Preview rebound (not a fresh Start) onto the SPECIFIC Ethernet IP with auth so
    // the phone can identity-probe it before confirming (R6: rides the Ethernet path).
    EXPECT_EQ(streaming.app_rebinds, 1);
    EXPECT_EQ(streaming.last_config.address, "10.10.1.30");
    EXPECT_EQ(streaming.last_config.port, kPreviewPort);
    EXPECT_EQ(streaming.last_config.auth_credential, credential);
    // No radio / GO plumbing touched — Ethernet-as-LAN never forms a WiFi-Direct group.
    EXPECT_EQ(wifi.starts, 0);
    EXPECT_EQ(wifi.stops, 0);
    EXPECT_EQ(dhcp.starts, 0);
    EXPECT_EQ(dhcp.stops, 0);
    EXPECT_EQ(netcfg.clears, 0);
}

// U6: a rebind failure is degraded-but-not-fatal — the hook still returns the minted
// credential (the arbiter surfaces it; the phone probe simply fails and the arbiter
// de-selects per R19), matching the STA rebind's degraded-not-fatal contract.
TEST(WifiDirectHandlerTest, OnEthernetLanSelectedReturnsCredentialEvenWhenRebindFails) {
    FakeCleanup cleanup;
    sst::session::SessionManager manager(cleanup);
    manager.OnConnect();
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    FakeStreaming streaming;
    streaming.app_start_ok = false;  // RebindAppStream fails
    WifiDirectHandler handler(manager, wifi, netcfg, dhcp, streaming,
                              sst::control::PreviewPort{kPreviewPort},
                              sst::control::DownloadPort{kDownloadPort});

    const std::string credential = handler.OnEthernetLanSelected("10.10.1.30");

    EXPECT_FALSE(credential.empty());  // still minted despite the rebind failure
    EXPECT_EQ(streaming.app_rebinds, 1);
}

}  // namespace
