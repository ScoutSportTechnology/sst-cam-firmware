// OpenCV JPEG thumbnail writer (U10). Runs in-container (CPU OpenCV).

#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

#include "../support/temp-dir.hpp"
#include "adapters/storage/opencv/opencv-thumbnail-writer.hpp"
#include "domain/capture/models/frame.hpp"

namespace fs = std::filesystem;

namespace {

// A BGR frame is encoded to a non-empty JPEG on disk.
TEST(ThumbnailWriterTest, WritesBgrFrameToJpeg) {
    constexpr int kWidth = 16;
    constexpr int kHeight = 16;
    constexpr int kBgrChannels = 3;
    constexpr std::uint8_t kFill = 120;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kWidth) * kHeight * kBgrChannels,
                                     kFill);

    sst::capture::Frame frame;
    frame.geometry = {.width = kWidth, .height = kHeight};
    frame.format = sst::common::PixelFormat::BGR8;
    frame.planes.push_back(
        {.stride = kWidth * kBgrChannels, .data = pixels.data(), .size = pixels.size()});

    sst::test::TempDir dir("thumb");
    const fs::path out = dir.path() / "test.jpg";
    sst::adapters::storage::OpenCvThumbnailWriter writer;
    ASSERT_TRUE(writer.Write(frame, out));
    ASSERT_TRUE(fs::exists(out));
    EXPECT_GT(fs::file_size(out), 0U);
}

// A frame with no pixel data fails cleanly.
TEST(ThumbnailWriterTest, EmptyFrameFails) {
    constexpr std::uint32_t kDim = 16;
    sst::capture::Frame frame;
    frame.geometry = {.width = kDim, .height = kDim};
    frame.format = sst::common::PixelFormat::BGR8;
    sst::test::TempDir dir("thumb");
    sst::adapters::storage::OpenCvThumbnailWriter writer;
    EXPECT_FALSE(writer.Write(frame, dir.path() / "test.jpg"));
}

}  // namespace
