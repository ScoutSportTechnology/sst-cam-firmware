#include "adapters/capture/frame/gstreamer/gstreamer.hpp"

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include "app/config/services/config_loader/config-loader.hpp"
#include "domain/capture/models/camera-config.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kConfigDir = SST_REPO_ROOT_DIR "/tests/config/config_files";

}  // namespace

TEST(GstreamerAdapter, DefaultPipelineUses60HzAntibandingForBothSensors) {
    const char* previous = ::getenv("SST_ISP_TUNING");
    const std::string saved = previous == nullptr ? "" : previous;
    ASSERT_EQ(::unsetenv("SST_ISP_TUNING"), 0);
    for (int sensor_id : {0, 1}) {
        sst::capture::GStreamerAdapter adapter({}, "v1", static_cast<std::uint16_t>(sensor_id));
        const std::string pipeline = adapter.CreatePipeline();
        EXPECT_NE(pipeline.find("nvarguscamerasrc sensor-id=" + std::to_string(sensor_id) +
                                " tnr-mode=2 tnr-strength=0.9 ee-mode=1 ee-strength=0.4 "
                                "aeantibanding=3 !"),
                  std::string::npos);
    }
    ASSERT_EQ(::setenv("SST_ISP_TUNING", "tnr-mode=1", 1), 0);
    sst::capture::GStreamerAdapter override_adapter({}, "v1", 0);
    const std::string override_pipeline = override_adapter.CreatePipeline();
    EXPECT_NE(override_pipeline.find("sensor-id=0 tnr-mode=1 !"), std::string::npos);
    EXPECT_EQ(override_pipeline.find("aeantibanding=3"), std::string::npos);
    if (previous == nullptr) {
        EXPECT_EQ(::unsetenv("SST_ISP_TUNING"), 0);
    } else {
        EXPECT_EQ(::setenv("SST_ISP_TUNING", saved.c_str(), 1), 0);
    }
}

// Capture test runs against real Jetson hardware (IMX477 via nvarguscamerasrc).
// Build in dev container, scp the test binary to the Jetson, run with:
//   ./sst_cam_firmware_tests --gtest_filter=GstreamerAdapter.*
TEST(GstreamerAdapter, CaptureSingleFrameAndLog) {
    sst::config::app::ConfigLoader loader(kConfigDir, "json");
    sst::config::ConfigData cfg = loader.get();
    const std::string device_model = cfg.device.model.value_or("");

    // Camera sensor config is a compiled-in default now (app-as-source-of-truth:
    // no local DB). Lens calibration still comes from calibration.json.
    const sst::capture::CameraConfig camera_cfg{};

    constexpr std::uint16_t kCameraIndex = 0;
    sst::capture::GStreamerAdapter adapter(camera_cfg, device_model, kCameraIndex);
    adapter.Start();
    ASSERT_TRUE(adapter.IsRunning()) << "GStreamer pipeline did not start";

    std::optional<sst::capture::Frame> frame;
    std::optional<sst::capture::Frame> prev;

    constexpr int kMaxSeconds = 5;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(kMaxSeconds);

    spdlog::info("Starting frame capture loop...");

    while (std::chrono::steady_clock::now() < deadline) {
        auto captured = adapter.Capture();
        if (!captured) {
            std::this_thread::yield();
            continue;
        }

        frame = *captured;

        if (prev) {
            auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                frame->captured_at - prev->captured_at)
                                .count();
            if (delta_ms > 0) {
                constexpr double kMillisecondsPerSecond = 1000.0;
                double fps = kMillisecondsPerSecond / static_cast<double>(delta_ms);
                spdlog::info("frame_id={} dt={} ms (~{:.1f} fps)", frame->frame_id, delta_ms, fps);
            }
        }

        prev = *frame;
    }

    spdlog::info("Frame capture loop ended.");

    if (!frame) {
        FAIL() << "Failed to capture frame from GStreamer";
        return;
    }

    const auto ts_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(frame->captured_at.time_since_epoch())
            .count();

    spdlog::info("Captured frame:");
    spdlog::info("  frame_id      = {}", frame->frame_id);
    spdlog::info("  timestamp_ms  = {}", ts_ms);
    spdlog::info("  width         = {}", frame->geometry.width);
    spdlog::info("  height        = {}", frame->geometry.height);
    spdlog::info("  planes        = {}", frame->planes.size());
    spdlog::info("  bytes         = {}", frame->planes[0].size);
    spdlog::info("  stride        = {}", frame->planes[0].stride);
    spdlog::info("  pixel_format  = {}", static_cast<int>(frame->format));

    adapter.Stop();
    EXPECT_FALSE(adapter.IsRunning());
}
