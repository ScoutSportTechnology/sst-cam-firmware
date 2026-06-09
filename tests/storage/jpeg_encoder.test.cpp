// OpenCV in-memory JPEG encoder (U9). Runs in-container (CPU OpenCV). Encodes a
// frame to JPEG bytes for the on-demand BLE ThumbnailRequest path.

#include "adapters/storage/opencv/opencv-jpeg-encoder.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include <opencv2/imgcodecs.hpp>

#include "domain/capture/models/frame.hpp"

namespace {

using sst::adapters::storage::OpenCvJpegEncoder;

auto MakeBgrFrame(int w, int h, std::uint8_t value) -> std::pair<sst::capture::Frame,
                                                                  std::vector<std::uint8_t>> {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 3, value);
    sst::capture::Frame frame;
    frame.geometry = {.width = static_cast<std::uint32_t>(w),
                      .height = static_cast<std::uint32_t>(h)};
    frame.format = sst::common::PixelFormat::BGR8;
    frame.planes.push_back(
        {.stride = static_cast<std::uint32_t>(w * 3), .data = pixels.data(), .size = pixels.size()});
    return {frame, std::move(pixels)};
}

// A BGR frame encodes to a JPEG that decodes back at the source dimensions.
TEST(JpegEncoderTest, EncodesBgrFrameToJpegBytes) {
    auto [frame, storage] = MakeBgrFrame(32, 24, 120);
    OpenCvJpegEncoder encoder;
    auto bytes = encoder.Encode(frame, /*width=*/0, /*height=*/0, /*quality=*/0);
    ASSERT_TRUE(bytes.has_value());
    ASSERT_GE(bytes->size(), 2U);
    EXPECT_EQ((*bytes)[0], 0xFF);  // JPEG Start Of Image
    EXPECT_EQ((*bytes)[1], 0xD8);

    // Decode the output and assert it is a real JPEG of the source size.
    const cv::Mat decoded = cv::imdecode(cv::Mat(*bytes), cv::IMREAD_COLOR);
    ASSERT_FALSE(decoded.empty()) << "encoded bytes did not decode as a JPEG";
    EXPECT_EQ(decoded.cols, 32);
    EXPECT_EQ(decoded.rows, 24);
}

// A requested output size resizes before encoding — the decoded JPEG carries the
// requested dimensions, not the source's.
TEST(JpegEncoderTest, ResizesToRequestedDimensions) {
    auto [frame, storage] = MakeBgrFrame(64, 64, 200);
    OpenCvJpegEncoder encoder;
    auto small = encoder.Encode(frame, /*width=*/16, /*height=*/16, /*quality=*/80);
    ASSERT_TRUE(small.has_value());
    EXPECT_FALSE(small->empty());

    const cv::Mat decoded = cv::imdecode(cv::Mat(*small), cv::IMREAD_COLOR);
    ASSERT_FALSE(decoded.empty()) << "resized output did not decode as a JPEG";
    EXPECT_EQ(decoded.cols, 16);
    EXPECT_EQ(decoded.rows, 16);
}

// A frame with no pixel data fails cleanly (nullopt, no crash).
TEST(JpegEncoderTest, EmptyFrameReturnsNullopt) {
    sst::capture::Frame frame;
    frame.geometry = {.width = 16, .height = 16};
    frame.format = sst::common::PixelFormat::BGR8;
    OpenCvJpegEncoder encoder;
    EXPECT_FALSE(encoder.Encode(frame, 0, 0, 0).has_value());
}

}  // namespace
