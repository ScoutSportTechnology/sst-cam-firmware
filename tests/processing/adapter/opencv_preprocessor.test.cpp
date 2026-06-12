#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <utility>

#include "../synthetic_frames.hpp"
#include "adapters/processing/opencv/opencv-preprocessor.hpp"
#include "domain/capture/models/frame.hpp"
#include "domain/common/models/pixel-format.hpp"
#include "domain/processing/models/color-mode.hpp"
#include "domain/processing/models/preprocess-config.hpp"

namespace sst::adapters::processing {

using sst::processing::ColorMode;
using sst::processing::PreprocessConfig;
using sst::tests::processing::MakeBgr8Frame;
using sst::tests::processing::MakeNv12Frame;

TEST(OpenCvPreprocessorTest, RejectsNonNv12) {
    OpenCvPreprocessor pre{PreprocessConfig{.ai_width = 32, .ai_height = 32}};
    auto out = pre.Process(MakeBgr8Frame(64, 64, 0, 0, 0));
    EXPECT_FALSE(out.has_value());
}

TEST(OpenCvPreprocessorTest, GrayscalePathDimensionsAndFormat) {
    OpenCvPreprocessor pre{
        PreprocessConfig{.ai_width = 32, .ai_height = 16, .ai_color_mode = ColorMode::Grayscale}};
    auto frame = MakeNv12Frame(64, 32, /*y=*/128, /*u=*/128, /*v=*/128);

    auto out = pre.Process(frame);
    ASSERT_TRUE(out.has_value());

    // source_frame is a value-copy of raw — still NV12.
    EXPECT_EQ(out->source_frame.format, sst::common::PixelFormat::NV12);
    EXPECT_EQ(out->source_frame.geometry.width, 64U);
    EXPECT_EQ(out->source_frame.geometry.height, 32U);
    EXPECT_EQ(out->source_frame.frame_id, frame.frame_id);

    // ai_frame is grayscale at the configured size.
    EXPECT_EQ(out->ai_frame.format, sst::common::PixelFormat::GRAY8);
    EXPECT_EQ(out->ai_frame.geometry.width, 32U);
    EXPECT_EQ(out->ai_frame.geometry.height, 16U);
    ASSERT_EQ(out->ai_frame.planes.size(), 1U);
    EXPECT_EQ(out->ai_frame.planes[0].stride, 32U);
    EXPECT_EQ(out->ai_frame.frame_id, frame.frame_id);
    EXPECT_EQ(out->ai_frame.captured_at, frame.captured_at);

    // All Y pixels were 128, so ai pixels should also be ~128.
    for (std::size_t i = 0; i < out->ai_frame.planes[0].size; ++i) {
        EXPECT_NEAR(out->ai_frame.planes[0].data[i], 128, 2);
    }
}

TEST(OpenCvPreprocessorTest, BinaryPathThresholdsHigh) {
    OpenCvPreprocessor pre{PreprocessConfig{.ai_width = 16,
                                            .ai_height = 16,
                                            .ai_color_mode = ColorMode::Binary,
                                            .binary_threshold = 127}};
    auto out = pre.Process(MakeNv12Frame(32, 32, /*y=*/200, /*u=*/128, /*v=*/128));
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->ai_frame.format, sst::common::PixelFormat::GRAY8);
    for (std::size_t i = 0; i < out->ai_frame.planes[0].size; ++i) {
        EXPECT_EQ(out->ai_frame.planes[0].data[i], 255);
    }
}

TEST(OpenCvPreprocessorTest, BinaryPathThresholdsLow) {
    OpenCvPreprocessor pre{PreprocessConfig{.ai_width = 16,
                                            .ai_height = 16,
                                            .ai_color_mode = ColorMode::Binary,
                                            .binary_threshold = 127}};
    auto out = pre.Process(MakeNv12Frame(32, 32, /*y=*/50, /*u=*/128, /*v=*/128));
    ASSERT_TRUE(out.has_value());
    for (std::size_t i = 0; i < out->ai_frame.planes[0].size; ++i) {
        EXPECT_EQ(out->ai_frame.planes[0].data[i], 0);
    }
}

TEST(OpenCvPreprocessorTest, RgbPathChannelsAndOrder) {
    OpenCvPreprocessor pre{
        PreprocessConfig{.ai_width = 8, .ai_height = 8, .ai_color_mode = ColorMode::RGB}};
    // Red-ish in YUV (BT.601): Y=76, U=84, V=255.
    auto out = pre.Process(MakeNv12Frame(32, 32, /*y=*/76, /*u=*/84, /*v=*/255));
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->ai_frame.format, sst::common::PixelFormat::RGB8);
    ASSERT_EQ(out->ai_frame.planes.size(), 1U);
    EXPECT_EQ(out->ai_frame.planes[0].stride, 8U * 3U);

    const auto* px = out->ai_frame.planes[0].data;
    // R channel should be high, B channel should be low.
    EXPECT_GT(px[0], 150);  // R
    EXPECT_LT(px[2], 80);   // B
}

TEST(OpenCvPreprocessorTest, StridedNv12RgbCopyFallback) {
    OpenCvPreprocessor pre{
        PreprocessConfig{.ai_width = 16, .ai_height = 16, .ai_color_mode = ColorMode::RGB}};
    constexpr std::uint32_t kW = 32;
    constexpr std::uint32_t kH = 32;
    constexpr std::uint32_t kStride = kW + 16;

    auto strided = MakeNv12Frame(kW, kH, 100, 128, 128, kStride);
    auto contig = MakeNv12Frame(kW, kH, 100, 128, 128);

    auto out_strided = pre.Process(strided);
    auto out_contig = pre.Process(contig);

    ASSERT_TRUE(out_strided.has_value());
    ASSERT_TRUE(out_contig.has_value());
    ASSERT_EQ(out_strided->ai_frame.planes[0].size, out_contig->ai_frame.planes[0].size);

    // Pixels from strided input must match the contiguous reference.
    EXPECT_EQ(0,
              std::memcmp(out_strided->ai_frame.planes[0].data, out_contig->ai_frame.planes[0].data,
                          out_strided->ai_frame.planes[0].size));
}

TEST(OpenCvPreprocessorTest, SourceFrameSharesOwnerWithRaw) {
    OpenCvPreprocessor pre{PreprocessConfig{.ai_width = 16, .ai_height = 16}};
    auto raw = MakeNv12Frame(32, 32, 128, 128, 128);

    const auto before = raw.owner.use_count();
    auto out = pre.Process(raw);
    ASSERT_TRUE(out.has_value());

    EXPECT_EQ(raw.owner.use_count(), before + 1);
    EXPECT_EQ(out->source_frame.owner, raw.owner);
    // Plane data pointers should be identical (no copy).
    EXPECT_EQ(out->source_frame.planes[0].data, raw.planes[0].data);
    EXPECT_EQ(out->source_frame.planes[1].data, raw.planes[1].data);
}

TEST(OpenCvPreprocessorTest, AiFrameOwnerOutlivesRaw) {
    OpenCvPreprocessor pre{PreprocessConfig{.ai_width = 16, .ai_height = 16}};
    auto out = [&] {
        auto raw = MakeNv12Frame(32, 32, 200, 128, 128);
        auto bundle = pre.Process(raw);
        // Manually drop the source_frame's owner so the GstBuffer-equivalent
        // backing is gone. ai_frame must still be readable.
        bundle->source_frame.owner.reset();
        return bundle;
    }();

    ASSERT_TRUE(out.has_value());
    ASSERT_NE(out->ai_frame.owner, nullptr);
    for (std::size_t i = 0; i < out->ai_frame.planes[0].size; ++i) {
        EXPECT_NEAR(out->ai_frame.planes[0].data[i], 200, 2);
    }
}

}  // namespace sst::adapters::processing
