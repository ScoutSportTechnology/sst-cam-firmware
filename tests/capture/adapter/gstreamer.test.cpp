#include "capture/adapters/gstreamer.hpp"

#include <glib.h>
#include <gst/gst.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>

#include "config/app/config_loader.hpp"

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

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);

    spdlog::info("Starting frame capture loop...");

    while (std::chrono::steady_clock::now() < deadline) {
        auto capturedFrame = adapter.Capture();  // <-- don't shadow
        if (!capturedFrame) {
            std::this_thread::yield();
            continue;
        }

        frame = *capturedFrame;  // <-- store latest good frame

        spdlog::debug("Capture frame");

        if (prev) {
            spdlog::debug("Have previous frame to compute delta time");
            auto captureInterval = frame->captured_at - prev->captured_at;
            auto millisecondsCount =
                std::chrono::duration_cast<std::chrono::milliseconds>(captureInterval).count();
            spdlog::debug("Delta time: {} ms", millisecondsCount);
            if (millisecondsCount > 0) {
                double fps = 1000.0 / millisecondsCount;
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

    adapter.Stop();
    EXPECT_FALSE(adapter.IsRunning());
}
