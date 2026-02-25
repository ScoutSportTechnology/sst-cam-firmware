#include <glib.h>
#include <gst/gst.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>

#include "adapters/capture/frame/gstreamer/gstreamer.hpp"
#include "app/config/services/config_loader/config-loader.hpp"


namespace fs = std::filesystem;

namespace {
constexpr const char* kRootRel = "tests/config/config_files";

auto RootDir() -> fs::path { return fs::path{SST_REPO_ROOT_DIR} / kRootRel; }
}  // namespace

TEST(GstreamerAdapter, CaptureSingleFrameAndLog) {
    const fs::path root = RootDir();
    sst::config::app::ConfigLoader loader(root.string(), "json");
    sst::config::domain::ConfigData cfg = loader.get();

    constexpr std::uint16_t camera_index = 0;

    sst::capture::adapters::GStreamerAdapter adapter(cfg, camera_index);
    adapter.Start();
    ASSERT_TRUE(adapter.IsRunning()) << "GStreamer pipeline did not start";

    std::optional<sst::capture::domain::Frame> frame;
    std::optional<sst::capture::domain::Frame> prev;

    constexpr int kMaxSeconds = 5;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(kMaxSeconds);

    spdlog::info("Starting frame capture loop...");

    while (std::chrono::steady_clock::now() < deadline) {
        auto capturedFrame = adapter.Capture();
        if (!capturedFrame) {
            std::this_thread::yield();
            continue;
        }

        frame = *capturedFrame;

        spdlog::debug("Capture frame");

        if (prev) {
            auto captureInterval = frame->captured_at - prev->captured_at;
            auto millisecondsCount =
                std::chrono::duration_cast<std::chrono::milliseconds>(captureInterval).count();
            if (millisecondsCount > 0) {
                constexpr double kMillisecondsPerSecond = 1000.0;
                double fps = kMillisecondsPerSecond / static_cast<double>(millisecondsCount);
                spdlog::info("frame_id={} dt={} ms (~{:.1f} fps)", frame->frame_id,
                             millisecondsCount, fps);
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
