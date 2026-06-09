#include <filesystem>
#include <memory>
#include <vector>

#include <spdlog/spdlog.h>

#include "adapters/capture/frame/gstreamer/gstreamer.hpp"
#include "adapters/processing/opencv/opencv-postprocessor.hpp"
#include "adapters/processing/opencv/opencv-preprocessor.hpp"
#include "adapters/storage/filesystem/filesystem-disk-guard.hpp"
#include "adapters/storage/gstreamer/gst-continuous-recorder.hpp"
#include "adapters/storage/opencv/opencv-thumbnail-writer.hpp"
#include "adapters/streaming/gst_rtmp/gst-rtmp-streamer.hpp"
#include "adapters/streaming/gst_rtsp/gst-rtsp-app-stream-server.hpp"
#include "app/buffer/services/fan_out_sink/fan-out-sink.hpp"
#include "app/config/services/config_loader/config-loader.hpp"
#include "app/pipeline/services/orchestrator/pipeline-orchestrator.hpp"
#include "app/storage/services/recording_service/recording-service.hpp"
#include "app/streaming/services/streaming_service/streaming-service.hpp"
#include "domain/capture/models/camera-config.hpp"

// ============================================================
// Runtime paths — adjust per deployment environment
// ============================================================
namespace sst::paths {

constexpr const char* kConfigDir = "/etc/sst/cam/config";
constexpr const char* kConfigFormat = "json";
constexpr const char* kVideoRootFallback = "/var/lib/sst/cam/videos";

}  // namespace sst::paths

// Camera 0 is the only camera wired into the pipeline today. The second
// IMX477 + the AI/physics/decision modules will land in follow-up changes;
// when they do, the orchestrator constructor grows to accept the second
// capture + a decision step that picks which camera to postprocess.
namespace sst::pipeline_defaults {

constexpr std::uint16_t kCamera0Index = 0;

}  // namespace sst::pipeline_defaults

// App-as-source-of-truth refactor (big-bang, KTD9): the control plane (proto
// dispatcher + session SM + handlers + BLE/WiFi adapters) is intentionally
// absent here between the U2 teardown and the U14 re-wiring. This is a
// reduced-function intermediate state — config + pipeline + storage + streaming
// run; nothing is stubbed. The new control plane is assembled in U14.
auto main() -> int {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("sst-cam-firmware starting");

    // ── Config (device identity + lens calibration — the only persistent state) ─
    sst::config::app::ConfigLoader config(sst::paths::kConfigDir, sst::paths::kConfigFormat);
    auto cfg = config.get();

    // ── Storage module (single continuous MP4 per session) ─────────────
    const std::filesystem::path video_root =
        cfg.storage.video.value_or(sst::paths::kVideoRootFallback);
    sst::adapters::storage::FilesystemDiskGuard disk_guard(video_root, cfg.storage.min_free_bytes);
    sst::storage::RecordingService recording_service(
        std::make_unique<sst::adapters::storage::GstContinuousRecorder>(),
        std::make_unique<sst::adapters::storage::OpenCvThumbnailWriter>(), disk_guard);

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

    const sst::capture::CameraConfig camera_cfg{};
    auto capture = std::make_unique<sst::capture::GStreamerAdapter>(
        camera_cfg, cfg.device.model.value_or(""), sst::pipeline_defaults::kCamera0Index);
    auto preprocessor = std::make_unique<sst::adapters::processing::OpenCvPreprocessor>();
    auto postprocessor = std::make_unique<sst::adapters::processing::OpenCvPostprocessor>();
    sst::pipeline::PipelineOrchestrator pipeline(std::move(capture), std::move(preprocessor),
                                                 std::move(postprocessor), final_frame_sink);

    pipeline.Start();

    spdlog::info("startup complete");
    return 0;
}
