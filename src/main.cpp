#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "adapters/capture/frame/gstreamer/gstreamer.hpp"
#include "adapters/control/ble/bluez/bluez-ble-transport.hpp"
#include "adapters/control/wifi/wpa_supplicant/wpa-wifi-manager.hpp"
#include "adapters/processing/opencv/opencv-postprocessor.hpp"
#include "adapters/processing/opencv/opencv-preprocessor.hpp"
#include "adapters/storage/filesystem/filesystem-disk-guard.hpp"
#include "adapters/storage/gstreamer/gst-encoder-pipeline.hpp"
#include "adapters/storage/gstreamer/gst-event-clip-recorder.hpp"
#include "adapters/storage/gstreamer/gst-segment-recorder.hpp"
#include "adapters/streaming/gst_rtmp/gst-rtmp-streamer.hpp"
#include "adapters/streaming/gst_rtsp/gst-rtsp-app-stream-server.hpp"
#include "app/buffer/services/fan_out_sink/fan-out-sink.hpp"
#include "app/config/services/config_loader/config-loader.hpp"
#include "app/control/services/control_module/control-module.hpp"
#include "app/control/services/controllers/camera.controller.hpp"
#include "app/control/services/controllers/match.controller.hpp"
#include "app/control/services/controllers/network.controller.hpp"
#include "app/control/services/controllers/recording.controller.hpp"
#include "app/control/services/controllers/sport.controller.hpp"
#include "app/control/services/controllers/streaming.controller.hpp"
#include "app/control/services/controllers/system.controller.hpp"
#include "app/control/services/controllers/team.controller.hpp"
#include "app/db/services/db_manager/db-manager.hpp"
#include "app/db/services/db_seeder/db-seeder.hpp"
#include "app/match/services/match_service/match-service.hpp"
#include "app/pipeline/services/orchestrator/pipeline-orchestrator.hpp"
#include "app/storage/services/recording_service/recording-service.hpp"
#include "app/streaming/services/streaming_service/streaming-service.hpp"

// ============================================================
// Runtime paths — adjust per deployment environment
// ============================================================
namespace sst::paths {

constexpr const char* kDbFile = "/var/lib/sst/cam/data.db";
constexpr const char* kDbSchema = "/usr/share/sst/cam/schema.sql";

constexpr const char* kConfigDir = "/etc/sst/cam/config";
constexpr const char* kConfigFormat = "json";
constexpr const char* kVideoRootFallback = "/var/lib/sst/cam/videos";

}  // namespace sst::paths

// ============================================================
// Storage encoder defaults — match what postprocess emits today (BGR8 1280x720
// @ 30fps). Bitrate sized for NVENC budget on Orin Nano alongside the app +
// platform streams.
// ============================================================
namespace sst::storage_defaults {

constexpr int kEncoderWidth = 1280;
constexpr int kEncoderHeight = 720;
constexpr int kEncoderFramerate = 30;
constexpr int kEncoderBitrateBps = 4'000'000;
constexpr std::int64_t kEventClipMaxPreSeconds = 120;

}  // namespace sst::storage_defaults

// Camera 0 is the only camera wired into the pipeline today. The second
// IMX477 + the AI/physics/decision modules will land in follow-up changes;
// when they do, the orchestrator constructor grows to accept the second
// capture + a decision step that picks which camera to postprocess.
namespace sst::pipeline_defaults {

constexpr std::uint16_t kCamera0Index = 0;

}  // namespace sst::pipeline_defaults

auto main() -> int {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("sst-cam-firmware starting");

    constexpr std::int64_t kDefaultUserId = 1;

    // ── Config ─────────────────────────────────────────────────────────
    sst::config::app::ConfigLoader config(sst::paths::kConfigDir, sst::paths::kConfigFormat);
    auto cfg = config.get();

    // ── Database ───────────────────────────────────────────────────────
    sst::db::DbManager database({
        .db_path = sst::paths::kDbFile,
        .schema_path = sst::paths::kDbSchema,
    });
    sst::db::DbSeeder::seedIfEmpty(database, cfg);

    // ── Storage module ────────────────────────────────────────────────
    // Single shared NVENC encoder feeds the segment recorder (full-game
    // splitmuxsink) and the event-clip ring (rolling pre-event NAL deque).
    auto encoder = std::make_unique<sst::adapters::storage::GstEncoderPipeline>(
        sst::adapters::storage::GstEncoderPipeline::Config{
            .width = sst::storage_defaults::kEncoderWidth,
            .height = sst::storage_defaults::kEncoderHeight,
            .framerate = sst::storage_defaults::kEncoderFramerate,
            .raw_format = "BGR",
            .bitrate_bps = sst::storage_defaults::kEncoderBitrateBps,
            .iframe_interval_frames = sst::storage_defaults::kEncoderFramerate,
        });
    auto segment_recorder = std::make_unique<sst::adapters::storage::GstSegmentRecorder>();
    auto event_clip_recorder = std::make_unique<sst::adapters::storage::GstEventClipRecorder>(
        sst::adapters::storage::GstEventClipRecorder::Config{
            .max_pre_seconds = sst::storage_defaults::kEventClipMaxPreSeconds,
        });

    const std::filesystem::path video_root =
        cfg.storage.video.value_or(sst::paths::kVideoRootFallback);
    sst::adapters::storage::FilesystemDiskGuard disk_guard(video_root, cfg.storage.min_free_bytes);
    sst::storage::RecordingService recording_service(
        std::move(encoder), std::move(segment_recorder), std::move(event_clip_recorder),
        database.recordings(), disk_guard, video_root);

    // ── Match module ───────────────────────────────────────────────────
    sst::match::MatchService match_service(database.matches(), database.sports(),
                                           database.cameras(), recording_service,
                                           kDefaultUserId);

    // ── Streaming module ───────────────────────────────────────────────
    auto app_stream_server =
        std::make_unique<sst::adapters::streaming::GstRtspAppStreamServer>();
    sst::streaming::StreamingService streaming_service(
        std::move(app_stream_server),
        [] { return std::make_unique<sst::adapters::streaming::GstRtmpStreamer>(); });

    // ── Pipeline orchestration ────────────────────────────────────────
    // Capture (sensor 0) → preprocess → buffer → postprocess → fan-out into
    // both the storage RecordingService and the streaming StreamingService.
    sst::buffer::FanOutSink final_frame_sink(
        std::vector<sst::buffer::IFrameSink*>{&recording_service, &streaming_service});

    const auto camera_cfg = database.cameras().getConfig(kDefaultUserId);
    auto capture = std::make_unique<sst::capture::GStreamerAdapter>(
        camera_cfg.data, cfg.device.model.value_or(""),
        sst::pipeline_defaults::kCamera0Index);
    auto preprocessor = std::make_unique<sst::adapters::processing::OpenCvPreprocessor>();
    auto postprocessor = std::make_unique<sst::adapters::processing::OpenCvPostprocessor>();
    sst::pipeline::PipelineOrchestrator pipeline(std::move(capture), std::move(preprocessor),
                                                 std::move(postprocessor), final_frame_sink);

    // ── Control plane (BLE + WiFi-Direct) ──────────────────────────────
    auto ble_transport = std::make_unique<sst::adapters::control::BluezBleTransport>();
    auto wifi_manager = std::make_unique<sst::adapters::control::WpaWifiManager>("wlan0");

    sst::control::ControlModule control(std::move(ble_transport), std::move(wifi_manager));

    // ★ Extensibility point — register one IController per concern.
    control.ble().Register(std::make_shared<sst::control::NetworkController>(
        control.wifi(), database.network(), kDefaultUserId));
    control.ble().Register(std::make_shared<sst::control::StreamingController>(
        streaming_service, database.streams()));
    control.ble().Register(
        std::make_shared<sst::control::CameraController>(database.cameras(), kDefaultUserId));
    control.ble().Register(
        std::make_shared<sst::control::RecordingController>(recording_service));
    control.ble().Register(std::make_shared<sst::control::MatchController>(
        match_service, database.matches(), database.recordings(), kDefaultUserId));
    control.ble().Register(std::make_shared<sst::control::SportController>(database.sports()));
    control.ble().Register(std::make_shared<sst::control::TeamController>(database.teams()));
    control.ble().Register(std::make_shared<sst::control::SystemController>(cfg.device));

    pipeline.Start();
    control.Start();

    spdlog::info("startup complete");
    return 0;
}
