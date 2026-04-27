#include "adapters/capture/frame/gstreamer/gstreamer.hpp"

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

#include "app/config/services/config_loader/config-loader.hpp"
#include "app/db/services/db_manager/db-manager.hpp"
#include "app/db/services/db_seeder/db-seeder.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kSchemaPath = SST_REPO_ROOT_DIR "/db/schema.sql";
constexpr const char* kDbPath = SST_REPO_ROOT_DIR "/tests/db/data/test.db";
constexpr const char* kConfigDir = SST_REPO_ROOT_DIR "/tests/config/config_files";
constexpr int64_t kDefaultUserId = 1;

}  // namespace

// Capture test runs against real Jetson hardware (IMX477 via nvarguscamerasrc).
// Build in dev container, scp the test binary to the Jetson, run with:
//   ./sst_cam_firmware_tests --gtest_filter=GstreamerAdapter.*
TEST(GstreamerAdapter, CaptureSingleFrameAndLog) {
    sst::config::app::ConfigLoader loader(kConfigDir, "json");
    sst::config::ConfigData cfg = loader.get();
    const std::string device_model = cfg.device.model.value_or("");

    const fs::path db_path{kDbPath};
    fs::create_directories(db_path.parent_path());
    fs::remove(db_path);

    sst::db::DbManager database({.db_path = kDbPath, .schema_path = kSchemaPath});
    sst::db::DbSeeder::seedIfEmpty(database, cfg);

    auto camera_cfg = database.cameras().getConfig(kDefaultUserId);
    ASSERT_TRUE(camera_cfg.success) << "Failed to load CameraConfig from DB";

    constexpr std::uint16_t kCameraIndex = 0;
    sst::capture::GStreamerAdapter adapter(camera_cfg.data, device_model, kCameraIndex);
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
            auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
                          frame->captured_at - prev->captured_at)
                          .count();
            if (dt > 0) {
                constexpr double kMillisecondsPerSecond = 1000.0;
                double fps = kMillisecondsPerSecond / static_cast<double>(dt);
                spdlog::info("frame_id={} dt={} ms (~{:.1f} fps)", frame->frame_id, dt, fps);
            }
        }

        prev = *frame;
    }

    spdlog::info("Frame capture loop ended.");

    ASSERT_TRUE(frame.has_value()) << "Failed to capture frame from GStreamer";

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
