// OpenCV JPEG thumbnail writer (U10). Runs in-container (CPU OpenCV).

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

#include "adapters/storage/opencv/opencv-thumbnail-writer.hpp"
#include "domain/capture/models/frame.hpp"

namespace fs = std::filesystem;

namespace {

auto TempFile(const std::string& suffix) -> fs::path {
    static std::atomic<int> n{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() /
           ("sst_thumb_" + std::to_string(stamp) + "_" + std::to_string(n.fetch_add(1)) + suffix);
}

// A BGR frame is encoded to a non-empty JPEG on disk.
TEST(ThumbnailWriterTest, WritesBgrFrameToJpeg) {
    constexpr int kW = 16;
    constexpr int kH = 16;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kW) * kH * 3, 120);

    sst::capture::Frame frame;
    frame.geometry = {.width = kW, .height = kH};
    frame.format = sst::common::PixelFormat::BGR8;
    frame.planes.push_back({.stride = kW * 3, .data = pixels.data(), .size = pixels.size()});

    const fs::path out = TempFile(".jpg");
    sst::adapters::storage::OpenCvThumbnailWriter writer;
    ASSERT_TRUE(writer.Write(frame, out));
    ASSERT_TRUE(fs::exists(out));
    EXPECT_GT(fs::file_size(out), 0U);
    fs::remove(out);
}

// A frame with no pixel data fails cleanly.
TEST(ThumbnailWriterTest, EmptyFrameFails) {
    sst::capture::Frame frame;
    frame.geometry = {.width = 16, .height = 16};
    frame.format = sst::common::PixelFormat::BGR8;
    sst::adapters::storage::OpenCvThumbnailWriter writer;
    EXPECT_FALSE(writer.Write(frame, TempFile(".jpg")));
}

}  // namespace
