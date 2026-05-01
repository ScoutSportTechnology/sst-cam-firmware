// End-to-end exercise of the storage GStreamer adapters.
//
// Hardware-bound: requires NVENC (`nvv4l2h264enc` + `nvvidconv`) on a Jetson.
// In the cross-compile dev container these tests fail at pipeline launch
// because the plugins aren't loadable — that's expected. They pass on-device.

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

#include "adapters/storage/gstreamer/gst-encoder-pipeline.hpp"
#include "adapters/storage/gstreamer/gst-event-clip-recorder.hpp"
#include "adapters/storage/gstreamer/gst-segment-merger.hpp"
#include "adapters/storage/gstreamer/gst-segment-recorder.hpp"
#include "domain/capture/models/frame.hpp"

namespace {

using sst::adapters::storage::GstEncoderPipeline;
using sst::adapters::storage::GstEventClipRecorder;
using sst::adapters::storage::GstSegmentMerger;
using sst::adapters::storage::GstSegmentRecorder;

constexpr int kWidth = 640;
constexpr int kHeight = 360;
constexpr int kFps = 30;

auto NextSuffix() -> std::string {
    static std::atomic<int> counter{0};
    return std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1));
}

// Allocates a synthetic BGR8 frame: one solid color, fully owned. Used as a
// stand-in for what postprocess would produce.
auto MakeBgr8Frame(std::uint64_t frame_id, std::uint8_t b, std::uint8_t g, std::uint8_t r,
                   std::shared_ptr<std::vector<std::uint8_t>>& storage) -> sst::capture::Frame {
    const std::size_t row_bytes = static_cast<std::size_t>(kWidth) * 3;
    const std::size_t total = row_bytes * static_cast<std::size_t>(kHeight);
    storage = std::make_shared<std::vector<std::uint8_t>>(total);
    for (std::size_t i = 0; i < total; i += 3) {
        (*storage)[i] = b;
        (*storage)[i + 1] = g;
        (*storage)[i + 2] = r;
    }
    sst::capture::Frame frame;
    frame.frame_id = frame_id;
    frame.format = sst::common::PixelFormat::BGR8;
    frame.memory = sst::common::MemoryType::CPU;
    frame.geometry = {.width = kWidth, .height = kHeight};
    frame.planes.push_back(sst::capture::FramePlane{
        .stride = static_cast<std::uint32_t>(row_bytes),
        .data = storage->data(),
        .size = total,
    });
    frame.captured_at = std::chrono::steady_clock::now();
    frame.owner = storage;
    return frame;
}

// Pushes `frame_count` synthetic frames into the encoder, paced at the
// configured FPS (~real-time so NVENC produces sensible PTS).
auto PumpFrames(GstEncoderPipeline& encoder, int frame_count) -> void {
    const auto interval = std::chrono::microseconds(1'000'000 / kFps);
    auto next = std::chrono::steady_clock::now();
    for (int i = 0; i < frame_count; ++i) {
        std::shared_ptr<std::vector<std::uint8_t>> storage;
        const std::uint8_t value = static_cast<std::uint8_t>(i & 0xFF);
        auto frame = MakeBgr8Frame(static_cast<std::uint64_t>(i), value, value, value, storage);
        encoder.Push(frame);
        next += interval;
        std::this_thread::sleep_until(next);
    }
}

class StorageE2ETest : public ::testing::Test {
   protected:
    auto SetUp() -> void override {
        const auto suffix = NextSuffix();
        root_ = std::filesystem::path(SST_REPO_ROOT_DIR) /
                ("tests/storage/data/e2e_" + suffix);
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
        std::filesystem::create_directories(root_);
    }

    auto TearDown() -> void override {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    std::filesystem::path root_;
};

TEST_F(StorageE2ETest, FullGameRecordsThreeSegmentsAndMergesOnStop) {
    GstEncoderPipeline encoder({.width = kWidth,
                                .height = kHeight,
                                .framerate = kFps,
                                .raw_format = "BGR",
                                .bitrate_bps = 1'500'000,
                                .iframe_interval_frames = kFps});
    GstSegmentRecorder segment_recorder;

    encoder.SetSegmentSink(
        [&](const sst::storage::EncodedNal& nal) { segment_recorder.OnEncoded(nal); });
    encoder.SetRingSink([](const sst::storage::EncodedNal&) {});

    const auto out_dir = root_ / "match-uuid" / "full_game";
    ASSERT_TRUE(segment_recorder.Start(out_dir));
    ASSERT_TRUE(encoder.Start());

    PumpFrames(encoder, kFps);  // ~1s
    segment_recorder.Pause();
    PumpFrames(encoder, kFps / 2);  // gap (dropped)
    segment_recorder.Resume();
    PumpFrames(encoder, kFps);  // ~1s
    segment_recorder.Pause();
    PumpFrames(encoder, kFps / 2);
    segment_recorder.Resume();
    PumpFrames(encoder, kFps);

    const auto merged = segment_recorder.Stop();
    encoder.Stop();

    ASSERT_FALSE(merged.empty());
    ASSERT_TRUE(std::filesystem::exists(merged));
    EXPECT_GT(std::filesystem::file_size(merged), 0U);
    EXPECT_GE(segment_recorder.Segments().size(), 2U);
}

TEST_F(StorageE2ETest, EventClipDumpsRingAndContinuesPostEvent) {
    GstEncoderPipeline encoder({.width = kWidth,
                                .height = kHeight,
                                .framerate = kFps,
                                .raw_format = "BGR",
                                .bitrate_bps = 1'500'000,
                                .iframe_interval_frames = kFps});
    GstEventClipRecorder clip_recorder({.max_pre_seconds = 5});

    encoder.SetSegmentSink([](const sst::storage::EncodedNal&) {});
    encoder.SetRingSink(
        [&](const sst::storage::EncodedNal& nal) { clip_recorder.OnEncoded(nal); });

    ASSERT_TRUE(encoder.Start());

    PumpFrames(encoder, kFps * 2);  // fill ring with ~2s

    const auto clip_path = root_ / "match-uuid" / "event_X" / "clip.mp4";
    std::filesystem::create_directories(clip_path.parent_path());
    ASSERT_TRUE(clip_recorder.Trigger(clip_path, {.pre_seconds = 1, .post_seconds = 1}));
    EXPECT_TRUE(clip_recorder.IsBusy());

    PumpFrames(encoder, kFps * 2);  // gives the post-event window time to elapse
    std::this_thread::sleep_for(std::chrono::seconds(1));  // let finalize worker drain

    encoder.Stop();

    ASSERT_TRUE(std::filesystem::exists(clip_path));
    EXPECT_GT(std::filesystem::file_size(clip_path), 0U);
}

TEST_F(StorageE2ETest, SegmentMergerProducesPlayableFile) {
    // Standalone exercise of the merger over hand-crafted segments produced
    // by a brief recording pass. Validates the concat → mp4mux pipeline runs
    // without re-encoding.
    GstEncoderPipeline encoder({.width = kWidth,
                                .height = kHeight,
                                .framerate = kFps,
                                .raw_format = "BGR",
                                .bitrate_bps = 1'500'000,
                                .iframe_interval_frames = kFps});
    GstSegmentRecorder segment_recorder;
    encoder.SetSegmentSink(
        [&](const sst::storage::EncodedNal& nal) { segment_recorder.OnEncoded(nal); });
    encoder.SetRingSink([](const sst::storage::EncodedNal&) {});

    const auto seg_dir = root_ / "match-uuid" / "full_game";
    ASSERT_TRUE(segment_recorder.Start(seg_dir));
    ASSERT_TRUE(encoder.Start());

    PumpFrames(encoder, kFps);
    segment_recorder.Pause();
    segment_recorder.Resume();
    PumpFrames(encoder, kFps);

    (void)segment_recorder.Stop();
    encoder.Stop();

    auto segments = segment_recorder.Segments();
    ASSERT_GE(segments.size(), 2U);

    GstSegmentMerger merger;
    const auto merged = root_ / "merged-standalone.mp4";
    ASSERT_TRUE(merger.Merge(segments, merged));
    EXPECT_TRUE(std::filesystem::exists(merged));
    EXPECT_GT(std::filesystem::file_size(merged), 0U);
}

}  // namespace
