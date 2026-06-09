// Continuous MP4 recorder real-encode (U10, R21, AE4). Hardware-bound: needs
// NVENC (nvv4l2h264enc/nvvidconv) — expected to FAIL in the container, passes
// on-device, where it produces a single playable MP4.

#include "adapters/storage/gstreamer/gst-continuous-recorder.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

#include "domain/capture/models/frame.hpp"

namespace fs = std::filesystem;

namespace {

TEST(GstContinuousE2E, EncodesSinglePlayableMp4) {
    const fs::path out = fs::temp_directory_path() / "sst_e2e_continuous.mp4";
    fs::remove(out);

    sst::adapters::storage::GstContinuousRecorder recorder;
    ASSERT_TRUE(recorder.Start(out)) << "NVENC pipeline did not start (expected off-device)";

    constexpr int kW = 1280;
    constexpr int kH = 720;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kW) * kH * 3, 0x40);
    for (int i = 0; i < 30; ++i) {
        sst::capture::Frame frame;
        frame.geometry = {.width = kW, .height = kH};
        frame.format = sst::common::PixelFormat::BGR8;
        frame.planes.push_back({.stride = kW * 3, .data = pixels.data(), .size = pixels.size()});
        recorder.Push(frame);
    }

    EXPECT_TRUE(recorder.Stop());
    EXPECT_TRUE(fs::exists(out));
    EXPECT_GT(fs::file_size(out), 0U);
    fs::remove(out);
}

}  // namespace
