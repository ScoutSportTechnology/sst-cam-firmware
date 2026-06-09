#include "adapters/processing/opencv/frame-mat.hpp"

#include <gtest/gtest.h>

#include <cstdint>

#include <opencv2/core.hpp>

#include "domain/capture/models/frame.hpp"
#include "domain/common/models/pixel-format.hpp"
#include "domain/common/models/timestamp.hpp"
#include "../synthetic_frames.hpp"

namespace sst::adapters::processing {

using sst::tests::processing::MakeNv12Frame;

TEST(FrameMatTest, WrapNv12RejectsNonNv12) {
    auto frame = sst::tests::processing::MakeBgr8Frame(8, 8, 0, 0, 0);
    EXPECT_FALSE(WrapNv12(frame).has_value());
}

TEST(FrameMatTest, WrapNv12RejectsOddDimensions) {
    auto frame = MakeNv12Frame(7, 8, 128, 128, 128);
    EXPECT_FALSE(WrapNv12(frame).has_value());
}

TEST(FrameMatTest, WrapNv12FastPathPointsAtInputMemory) {
    auto frame = MakeNv12Frame(64, 32, 100, 128, 128);

    auto wrapped = WrapNv12(frame);
    ASSERT_TRUE(wrapped.has_value());
    EXPECT_EQ(wrapped->rows, 48);  // H * 3/2
    EXPECT_EQ(wrapped->cols, 64);
    EXPECT_EQ(wrapped->type(), CV_8UC1);
    // Fast path: Mat row 0 must point at the input Y plane.
    EXPECT_EQ(wrapped->ptr(0), frame.planes[0].data);
}

TEST(FrameMatTest, WrapNv12CopiesWhenStrided) {
    constexpr std::uint32_t kW = 32;
    constexpr std::uint32_t kH = 16;
    constexpr std::uint32_t kStride = kW + 16;
    auto frame = MakeNv12Frame(kW, kH, /*y=*/77, /*u=*/40, /*v=*/200, kStride);

    auto wrapped = WrapNv12(frame);
    ASSERT_TRUE(wrapped.has_value());
    EXPECT_EQ(wrapped->rows, 24);
    EXPECT_EQ(wrapped->cols, 32);
    // Fallback path: Mat must NOT point at the strided input.
    EXPECT_NE(wrapped->ptr(0), frame.planes[0].data);

    // Y rows = 77 in the meaningful area.
    for (int row = 0; row < static_cast<int>(kH); ++row) {
        for (int col = 0; col < static_cast<int>(kW); ++col) {
            EXPECT_EQ(wrapped->ptr(row)[col], 77);
        }
    }
    // UV rows alternate 40, 200.
    for (int row = static_cast<int>(kH); row < wrapped->rows; ++row) {
        for (int col = 0; col < static_cast<int>(kW); col += 2) {
            EXPECT_EQ(wrapped->ptr(row)[col + 0], 40);
            EXPECT_EQ(wrapped->ptr(row)[col + 1], 200);
        }
    }
}

TEST(FrameMatTest, MakeOwnedFrameProducesCompactStride) {
    cv::Mat src(10, 20, CV_8UC3, cv::Scalar(11, 22, 33));

    auto out = MakeOwnedFrame(src, sst::common::PixelFormat::BGR8, 99,
                              sst::common::Timestamp{});

    EXPECT_EQ(out.frame_id, 99U);
    EXPECT_EQ(out.format, sst::common::PixelFormat::BGR8);
    EXPECT_EQ(out.geometry.width, 20U);
    EXPECT_EQ(out.geometry.height, 10U);
    ASSERT_EQ(out.planes.size(), 1U);
    EXPECT_EQ(out.planes[0].stride, 20U * 3U);
    EXPECT_EQ(out.planes[0].size, 10U * 20U * 3U);
    ASSERT_NE(out.owner, nullptr);

    // Spot-check pixel values.
    EXPECT_EQ(out.planes[0].data[0], 11);
    EXPECT_EQ(out.planes[0].data[1], 22);
    EXPECT_EQ(out.planes[0].data[2], 33);
}

}  // namespace sst::adapters::processing
