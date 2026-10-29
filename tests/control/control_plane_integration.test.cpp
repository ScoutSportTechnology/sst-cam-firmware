// Assembled control-plane smoke test (U14): commands routed through the real
// CommandDispatcher + SessionManager + per-concern handlers advance the F1
// lifecycle and produce the right responses. Pure — hardware bits are faked.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/control/ports/dhcp-server.hpp"
#include "app/control/ports/network-configurator.hpp"
#include "app/control/ports/wifi-manager.hpp"
#include "app/control/services/dispatcher/command-dispatcher.hpp"
#include "app/control/services/handlers/match-state.handler.hpp"
#include "app/control/services/handlers/overlay.handler.hpp"
#include "app/control/services/handlers/session.handler.hpp"
#include "app/control/services/handlers/wifi-direct.handler.hpp"
#include "app/overlay/ports/overlay-renderer.hpp"
#include "app/overlay/ports/overlay-sink.hpp"
#include "app/overlay/services/overlay_controller/overlay-controller.hpp"
#include "app/session/ports/session-cleanup.hpp"
#include "app/session/services/session_manager/session-manager.hpp"
#include "app/streaming/ports/streaming-service.hpp"
#include "bluetooth.pb.h"

namespace {

namespace control = sst::control;
namespace network = sst::network;
namespace overlay = sst::overlay;
namespace session = sst::session;
namespace streaming = sst::streaming;

// Overlay canvas edge (px) the controller renders at in this smoke test.
constexpr std::uint32_t kCanvasEdge = 64;
// RGBA bytes-per-pixel; stride = width * this.
constexpr std::uint32_t kRgbaBytesPerPixel = 4;
// RTSP preview + HTTP download ports advertised by the WiFi-Direct handler.
constexpr std::uint32_t kPreviewPort = 8554;
constexpr std::uint32_t kDownloadPort = 8080;
// Overlay layout canvas width pushed in the lifecycle test.
constexpr std::uint32_t kLayoutCanvasWidth = 1920;
constexpr auto kElapsedRecording = std::chrono::seconds(65);
constexpr std::uint32_t kCustomStopMinutes = 5;

class FakeCleanup final : public session::ISessionCleanup {
   public:
    auto FinalizeRecording() -> bool override {
        if (on_finalize) {
            on_finalize();
        }
        return true;
    }
    std::function<void()> on_finalize;
    auto StopStreaming() -> void override {}
    auto TeardownWifiDirect() -> void override {}
    auto ResetSelections() -> void override {}
};
class FakeWifi final : public control::IWifiManager {
   public:
    auto StartP2pGroupOwner() -> std::optional<network::WifiDirectGroup> override {
        network::WifiDirectGroup group;
        group.ssid = "DIRECT-z";
        group.psk = "pw";
        group.group_interface = "p2p-wlan0-0";
        group.group_owner_ip = "192.168.49.1";
        group.role = "GO";
        return group;
    }
    auto Stop() -> void override {}
    [[nodiscard]] auto State() const -> control::WifiState override { return {}; }
};
class FakeDhcp final : public control::IDhcpServer {
   public:
    auto Start(const std::string& /*group_interface*/,
               const std::string& /*go_ip*/) -> bool override {
        return true;
    }
    auto Stop() -> void override {}
};
class FakeNetworkConfigurator final : public control::INetworkConfigurator {
   public:
    auto AssignGroupOwnerAddress(const std::string& /*iface*/,
                                 const std::string& /*cidr*/) -> bool override {
        return true;
    }
    auto Clear(const std::string& /*iface*/) -> void override {}
};
class FakeRenderer final : public overlay::IOverlayRenderer {
   public:
    // Signature is fixed by the IOverlayRenderer port (external header); the
    // override must mirror its two adjacent std::uint32_t params verbatim.
    auto Render(const overlay::RenderScene& /*scene*/,
                std::uint32_t out_width,  // NOLINT(bugprone-easily-swappable-parameters) floor-ok:
                                          // test double; order fixed by IOverlayRenderer::Render,
                                          // cannot reorder in override
                std::uint32_t out_height) -> overlay::RgbaImage override {
        overlay::RgbaImage img;
        img.width = out_width;
        img.height = out_height;
        img.stride = out_width * kRgbaBytesPerPixel;
        img.pixels.assign(static_cast<std::size_t>(img.stride) * out_height, 0);
        return img;
    }
};
class FakeSink final : public overlay::IOverlaySink {
   public:
    auto PushFrame(const overlay::RgbaImage& /*frame*/) -> void override {}
};
class FakeStreaming final : public streaming::IStreamingService {
   public:
    auto StartAppStream(const streaming::AppStreamConfig& /*config*/) -> bool override {
        return true;
    }
    auto StopAppStream() -> bool override { return true; }
    [[nodiscard]] auto IsAppStreamRunning() const -> bool override { return false; }
    auto StartPlatformStream(const streaming::PlatformStreamConfig& /*config*/) -> bool override {
        return true;
    }
    auto StopPlatformStream(std::int64_t /*stream_id*/) -> bool override { return true; }
    [[nodiscard]] auto ListActivePlatformStreams() const
        -> std::vector<streaming::ActivePlatformStream> override {
        return {};
    }
};

auto Corr(const std::string& corr_id) { return corr_id; }

// Each lifecycle step is its own helper so the smoke test's per-step
// build/dispatch/assert sequences stay under the cognitive-complexity cap.

// StartWifiDirect -> WifiReady, with generated creds reported.
void StepStartWifiDirect(control::CommandDispatcher& dispatcher, session::SessionManager& manager) {
    sst_cam::Command cmd;
    cmd.set_correlation_id(Corr("a"));
    cmd.mutable_start_wifi_direct();
    auto resp = dispatcher.Dispatch(cmd);
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(resp.correlation_id(), "a");
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kWifiDirectGroup);
    EXPECT_EQ(resp.wifi_direct_group().ssid(), "DIRECT-z");
    EXPECT_EQ(manager.Phase(), session::SessionPhase::kWifiReady);
}

// PushSessionConfig -> Configured.
void StepPushSessionConfig(control::CommandDispatcher& dispatcher,
                           session::SessionManager& manager) {
    sst_cam::Command cmd;
    cmd.set_correlation_id(Corr("b"));
    auto* config = cmd.mutable_push_session_config();
    config->set_match_uuid("m");
    config->set_user_uuid("u");
    // empty output paths -> no dir creation
    auto resp = dispatcher.Dispatch(cmd);
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(manager.Phase(), session::SessionPhase::kConfigured);
}

// PushOverlayLayout -> Ready.
void StepPushOverlayLayout(control::CommandDispatcher& dispatcher,
                           session::SessionManager& manager) {
    sst_cam::Command cmd;
    cmd.set_correlation_id(Corr("c"));
    cmd.mutable_push_overlay_layout()->mutable_layout()->set_canvas_width(kLayoutCanvasWidth);
    auto resp = dispatcher.Dispatch(cmd);
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    EXPECT_EQ(manager.Phase(), session::SessionPhase::kReady);
}

// An unwired command still gets a defined UNSUPPORTED response.
void StepUnwiredCommandIsUnsupported(control::CommandDispatcher& dispatcher) {
    sst_cam::Command cmd;
    cmd.set_correlation_id(Corr("d"));
    cmd.mutable_factory_reset();
    auto resp = dispatcher.Dispatch(cmd);
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::UNSUPPORTED);
    EXPECT_EQ(resp.correlation_id(), "d");
}

TEST(ControlPlaneIntegrationTest, SnapshotReportsPhasesAndOptionalState) {
    FakeCleanup cleanup;
    session::SessionManager manager(cleanup);
    FakeRenderer renderer;
    FakeSink sink;
    overlay::OverlayController controller(renderer, sink,
                                          sst::common::OutputSize{kCanvasEdge, kCanvasEdge});
    control::CommandDispatcher dispatcher;
    dispatcher.Register(std::make_shared<control::SessionHandler>(manager, controller));
    sst_cam::Command command;
    command.mutable_get_session_snapshot();
    auto snapshot = dispatcher.Dispatch(command).session_snapshot();
    EXPECT_EQ(snapshot.session_phase(), sst_cam::SESSION_IDLE);
    EXPECT_FALSE(snapshot.has_match_state());
    EXPECT_FALSE(snapshot.has_last_session());
    manager.OnConnect();
    ASSERT_TRUE(manager.OnWifiReady());
    sst_cam::Command config;
    config.mutable_push_session_config()->set_match_uuid("match-42");
    EXPECT_EQ(dispatcher.Dispatch(config).status(), sst_cam::OK);
    snapshot = dispatcher.Dispatch(command).session_snapshot();
    EXPECT_EQ(snapshot.session_phase(), sst_cam::SESSION_CONFIGURED);
    ASSERT_TRUE(snapshot.has_match_state());
    EXPECT_EQ(snapshot.match_state().match_uuid(), "match-42");
    ASSERT_TRUE(manager.OnOverlayConfigured());
    EXPECT_EQ(dispatcher.Dispatch(command).session_snapshot().session_phase(),
              sst_cam::SESSION_READY);
    ASSERT_TRUE(manager.OnRecordingStart());
    snapshot = dispatcher.Dispatch(command).session_snapshot();
    EXPECT_EQ(snapshot.session_phase(), sst_cam::SESSION_RECORDING);
    EXPECT_TRUE(snapshot.is_recording());
    EXPECT_TRUE(snapshot.has_recording_elapsed_seconds());
    manager.OnDisconnect();
    EXPECT_EQ(dispatcher.Dispatch(command).session_snapshot().session_phase(),
              sst_cam::SESSION_RECORDING);
}

TEST(ControlPlaneIntegrationTest, ConfigPresenceAndRejoinSnapshotRetainActivity) {
    FakeCleanup cleanup;
    std::chrono::steady_clock::time_point now{};
    session::SessionManager manager(cleanup, [&] { return now; });
    FakeRenderer renderer;
    FakeSink sink;
    overlay::OverlayController controller(renderer, sink,
                                          sst::common::OutputSize{kCanvasEdge, kCanvasEdge});
    control::CommandDispatcher dispatcher;
    dispatcher.Register(std::make_shared<control::SessionHandler>(
        manager, controller, [] { return std::optional<std::uint32_t>(1); },
        [] { return std::optional<sst_cam::PreviewLayout>(sst_cam::PREVIEW_LAYOUT_SIDE_BY_SIDE); },
        [](std::uint32_t index) {
            return std::optional<sst_cam::CameraHealth>(
                index == 0 ? sst_cam::CAMERA_HEALTH_OK : sst_cam::CAMERA_HEALTH_RECOVERING);
        }));
    manager.OnConnect();
    ASSERT_TRUE(manager.OnWifiReady());
    sst_cam::Command config;
    config.mutable_push_session_config()->set_match_uuid("match-42");
    ASSERT_EQ(dispatcher.Dispatch(config).status(), sst_cam::OK);
    EXPECT_EQ(manager.Snapshot().config.value_or(session::SessionConfig{}).auto_stop_minutes,
              session::kDefaultAutoStopMinutes);
    ASSERT_TRUE(manager.OnOverlayConfigured());
    manager.OnStreamingChanged(true);
    ASSERT_TRUE(manager.OnRecordingStart());
    now += kElapsedRecording;
    manager.OnDisconnect();
    sst_cam::Command query;
    query.mutable_get_session_snapshot();
    const auto response = dispatcher.Dispatch(query);
    ASSERT_EQ(response.status(), sst_cam::OK);
    const auto& snapshot = response.session_snapshot();
    EXPECT_EQ(snapshot.session_phase(), sst_cam::SESSION_RECORDING);
    EXPECT_EQ(snapshot.active_camera_index(), 1U);
    EXPECT_EQ(snapshot.preview_layout(), sst_cam::PREVIEW_LAYOUT_SIDE_BY_SIDE);
    EXPECT_EQ(snapshot.camera0_health(), sst_cam::CAMERA_HEALTH_OK);
    EXPECT_EQ(snapshot.camera1_health(), sst_cam::CAMERA_HEALTH_RECOVERING);
    EXPECT_TRUE(snapshot.is_recording());
    EXPECT_TRUE(snapshot.is_streaming());
    EXPECT_TRUE(snapshot.wifi_group_up());
    EXPECT_EQ(snapshot.recording_elapsed_seconds(), 65U);
    EXPECT_FALSE(snapshot.has_last_session());
    ASSERT_TRUE(manager.OnConnect());
    EXPECT_EQ(dispatcher.Dispatch(query).session_snapshot().match_state().match_uuid(), "match-42");
}

TEST(ControlPlaneIntegrationTest, SameMatchConfigRejoinDoesNotClearOverlay) {
    FakeCleanup cleanup;
    session::SessionManager manager(cleanup);
    FakeRenderer renderer;
    FakeSink sink;
    overlay::OverlayController controller(renderer, sink,
                                          sst::common::OutputSize{kCanvasEdge, kCanvasEdge});
    control::SessionHandler handler(manager, controller);
    manager.OnConnect();
    ASSERT_TRUE(manager.OnWifiReady());
    sst_cam::Command config;
    config.mutable_push_session_config()->set_match_uuid("match-42");
    ASSERT_EQ(handler.Handle(config).status(), sst_cam::OK);
    ASSERT_TRUE(manager.OnOverlayConfigured());
    ASSERT_TRUE(manager.OnRecordingStart());
    manager.OnDisconnect();
    const auto before = controller.PushCount();
    ASSERT_TRUE(manager.OnConnect());
    EXPECT_EQ(handler.Handle(config).status(), sst_cam::OK);
    EXPECT_EQ(controller.PushCount(), before);
    EXPECT_EQ(manager.Phase(), session::SessionPhase::kRecording);
}

TEST(ControlPlaneIntegrationTest, ExplicitTimeoutValueRearmsWhileRecording) {
    FakeCleanup cleanup;
    std::chrono::steady_clock::time_point now{};
    session::SessionManager manager(cleanup, [&] { return now; });
    FakeRenderer renderer;
    FakeSink sink;
    overlay::OverlayController controller(renderer, sink,
                                          sst::common::OutputSize{kCanvasEdge, kCanvasEdge});
    control::CommandDispatcher dispatcher;
    dispatcher.Register(std::make_shared<control::SessionHandler>(manager, controller));
    manager.OnConnect();
    ASSERT_TRUE(manager.OnWifiReady());
    sst_cam::Command config;
    config.mutable_push_session_config()->set_match_uuid("match-42");
    ASSERT_EQ(dispatcher.Dispatch(config).status(), sst_cam::OK);
    ASSERT_TRUE(manager.OnOverlayConfigured());
    ASSERT_TRUE(manager.OnRecordingStart());
    manager.OnDisconnect();
    now += std::chrono::minutes(2);
    config.mutable_push_session_config()->set_auto_stop_minutes(kCustomStopMinutes);
    ASSERT_EQ(dispatcher.Dispatch(config).status(), sst_cam::OK);
    now += std::chrono::minutes(4);
    manager.Tick();
    EXPECT_EQ(manager.Phase(), session::SessionPhase::kRecording);
    now += std::chrono::minutes(1);
    manager.Tick();
    EXPECT_EQ(manager.Phase(), session::SessionPhase::kIdle);
}

TEST(ControlPlaneIntegrationTest, IdleSummaryMapsAutoStopAndIsAbsentWhenConfigured) {
    FakeCleanup cleanup;
    std::chrono::steady_clock::time_point now{};
    session::SessionManager manager(cleanup, [&] { return now; });
    FakeRenderer renderer;
    FakeSink sink;
    overlay::OverlayController controller(renderer, sink,
                                          sst::common::OutputSize{kCanvasEdge, kCanvasEdge});
    control::SessionHandler handler(manager, controller);
    manager.OnConnect();
    ASSERT_TRUE(manager.OnWifiReady());
    sst_cam::Command config;
    config.mutable_push_session_config()->set_match_uuid("match-42");
    ASSERT_EQ(handler.Handle(config).status(), sst_cam::OK);
    ASSERT_TRUE(manager.OnOverlayConfigured());
    ASSERT_TRUE(manager.OnRecordingStart());
    ASSERT_TRUE(
        manager.ApplyMatchUpdate([](session::LiveMatch& match) { match.clock_seconds = 47; }));
    manager.OnDisconnect();
    now += std::chrono::minutes(session::kDefaultAutoStopMinutes);
    manager.Tick();
    sst_cam::Command query;
    query.mutable_get_session_snapshot();
    auto snapshot = handler.Handle(query).session_snapshot();
    EXPECT_EQ(snapshot.session_phase(), sst_cam::SESSION_IDLE);
    ASSERT_TRUE(snapshot.has_last_session());
    EXPECT_EQ(snapshot.last_session().match_uuid(), "match-42");
    EXPECT_EQ(snapshot.last_session().end_reason(), sst_cam::SESSION_END_AUTO_STOP);
    EXPECT_EQ(snapshot.last_session().end_clock_seconds(), 47U);
    EXPECT_TRUE(snapshot.last_session().has_file_valid());
    manager.OnConnect();
    ASSERT_TRUE(manager.OnWifiReady());
    ASSERT_EQ(handler.Handle(config).status(), sst_cam::OK);
    EXPECT_FALSE(handler.Handle(query).session_snapshot().has_last_session());
}

TEST(ControlPlaneIntegrationTest, SnapshotReportsFinalizingDuringCleanup) {
    FakeCleanup cleanup;
    session::SessionManager manager(cleanup);
    FakeRenderer renderer;
    FakeSink sink;
    overlay::OverlayController controller(renderer, sink,
                                          sst::common::OutputSize{kCanvasEdge, kCanvasEdge});
    control::SessionHandler handler(manager, controller);
    manager.OnConnect();
    ASSERT_TRUE(manager.OnWifiReady());
    sst_cam::Command config;
    config.mutable_push_session_config()->set_match_uuid("match-42");
    ASSERT_EQ(handler.Handle(config).status(), sst_cam::OK);
    ASSERT_TRUE(manager.OnOverlayConfigured());
    ASSERT_TRUE(manager.OnRecordingStart());
    sst_cam::Command query;
    query.mutable_get_session_snapshot();
    cleanup.on_finalize = [&] {
        const auto snapshot = handler.Handle(query).session_snapshot();
        EXPECT_EQ(snapshot.session_phase(), sst_cam::SESSION_FINALIZING);
        EXPECT_FALSE(snapshot.has_last_session());
    };
    EXPECT_TRUE(manager.EndSession(session::SessionEndReason::kAppStop));
    EXPECT_EQ(handler.Handle(query).session_snapshot().session_phase(), sst_cam::SESSION_IDLE);
}

TEST(ControlPlaneIntegrationTest, RoutesFullLifecycleToReady) {
    FakeCleanup cleanup;
    session::SessionManager manager(cleanup);
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    FakeRenderer renderer;
    FakeSink sink;
    FakeStreaming streaming;
    overlay::OverlayController controller(renderer, sink,
                                          sst::common::OutputSize{kCanvasEdge, kCanvasEdge});

    control::CommandDispatcher dispatcher;
    dispatcher.Register(std::make_shared<control::SessionHandler>(manager, controller));
    dispatcher.Register(std::make_shared<control::WifiDirectHandler>(
        manager, wifi, netcfg, dhcp, streaming, sst::control::PreviewPort{kPreviewPort},
        sst::control::DownloadPort{kDownloadPort}));
    dispatcher.Register(std::make_shared<control::OverlayHandler>(manager, controller,
                                                                  [] { return std::uint64_t{0}; }));

    // The transport signals connect on the first write; simulate it.
    manager.OnConnect();

    StepStartWifiDirect(dispatcher, manager);
    StepPushSessionConfig(dispatcher, manager);
    StepPushOverlayLayout(dispatcher, manager);
    StepUnwiredCommandIsUnsupported(dispatcher);
}

// Out-of-order: PushSessionConfig before the WiFi group is up is rejected by the
// assembled plane (the SM gate surfaces as ERROR, never silence).
TEST(ControlPlaneIntegrationTest, OutOfOrderConfigRejected) {
    FakeCleanup cleanup;
    session::SessionManager manager(cleanup);
    FakeRenderer renderer;
    FakeSink sink;
    overlay::OverlayController controller(renderer, sink,
                                          sst::common::OutputSize{kCanvasEdge, kCanvasEdge});
    control::CommandDispatcher dispatcher;
    dispatcher.Register(std::make_shared<control::SessionHandler>(manager, controller));
    manager.OnConnect();

    sst_cam::Command cmd;
    cmd.set_correlation_id("x");
    cmd.mutable_push_session_config()->set_match_uuid("m");
    auto resp = dispatcher.Dispatch(cmd);
    EXPECT_EQ(resp.status(), sst_cam::ResponseStatus::ERROR);
    EXPECT_FALSE(resp.error_message().empty());
}

// #6 overlay model: configuring a match does NOT show a scoreboard (no active
// match yet -> no overlay). The board only appears at kickoff. The handler clears
// the overlay on config so a previous match's board can't linger.
TEST(ControlPlaneIntegrationTest, PushSessionConfigShowsNoPreKickoffOverlay) {
    FakeCleanup cleanup;
    session::SessionManager manager(cleanup);
    FakeWifi wifi;
    FakeDhcp dhcp;
    FakeNetworkConfigurator netcfg;
    FakeRenderer renderer;
    FakeSink sink;
    FakeStreaming streaming;
    overlay::OverlayController controller(renderer, sink,
                                          sst::common::OutputSize{kCanvasEdge, kCanvasEdge});
    control::CommandDispatcher dispatcher;
    dispatcher.Register(std::make_shared<control::SessionHandler>(manager, controller));
    dispatcher.Register(std::make_shared<control::WifiDirectHandler>(
        manager, wifi, netcfg, dhcp, streaming, sst::control::PreviewPort{kPreviewPort},
        sst::control::DownloadPort{kDownloadPort}));

    manager.OnConnect();
    StepStartWifiDirect(dispatcher, manager);
    EXPECT_EQ(controller.PushCount(), 0U);  // nothing rendered before any config
    StepPushSessionConfig(dispatcher, manager);
    EXPECT_EQ(controller.PushCount(), 0U);  // config clears; no board until kickoff
}

}  // namespace
