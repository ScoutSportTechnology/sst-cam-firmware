#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "domain/capture/models/frame.hpp"
#include "domain/overlay/models/render-scene.hpp"
#include "domain/overlay/services/frame-compositor.hpp"

namespace {

using sst::capture::Frame;
using sst::overlay::CompositeOverlay;
using sst::overlay::RgbaImage;

// Keep the backing buffer alive alongside the Frame that points into it.
struct BgrFrame {
    Frame frame;
    std::shared_ptr<std::vector<std::uint8_t>> bytes;
};

auto MakeBgr(std::uint32_t w, std::uint32_t h, std::uint8_t b, std::uint8_t g, std::uint8_t r)
    -> BgrFrame {
    auto buf = std::make_shared<std::vector<std::uint8_t>>(static_cast<std::size_t>(w) * h * 3);
    for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i) {
        (*buf)[i * 3 + 0] = b;
        (*buf)[i * 3 + 1] = g;
        (*buf)[i * 3 + 2] = r;
    }
    Frame f;
    f.format = sst::common::PixelFormat::BGR8;
    f.geometry = {.width = w, .height = h};
    f.planes = {
        sst::capture::FramePlane{.stride = w * 3, .data = buf->data(), .size = buf->size()}};
    f.owner = buf;
    return {std::move(f), buf};
}

auto MakeRgba(std::uint32_t w, std::uint32_t h, std::uint8_t r, std::uint8_t g, std::uint8_t b,
              std::uint8_t a) -> RgbaImage {
    RgbaImage img;
    img.width = w;
    img.height = h;
    img.stride = w * 4;
    img.pixels.resize(static_cast<std::size_t>(w) * h * 4);
    for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i) {
        img.pixels[i * 4 + 0] = r;
        img.pixels[i * 4 + 1] = g;
        img.pixels[i * 4 + 2] = b;
        img.pixels[i * 4 + 3] = a;
    }
    return img;
}

TEST(FrameCompositorTest, OpaqueOverlayReplacesPixels) {
    auto src = MakeBgr(4, 4, /*b=*/10, /*g=*/20, /*r=*/30);
    const auto overlay = MakeRgba(4, 4, /*r=*/200, /*g=*/100, /*b=*/50, /*a=*/255);

    auto out = CompositeOverlay(src.frame, overlay);
    ASSERT_TRUE(out.has_value());
    ASSERT_FALSE(out->planes.empty());
    const auto* px = out->planes[0].data;
    // Every pixel becomes the overlay color in BGR order (B=50, G=100, R=200).
    for (std::size_t i = 0; i < 16; ++i) {
        EXPECT_EQ(px[i * 3 + 0], 50);
        EXPECT_EQ(px[i * 3 + 1], 100);
        EXPECT_EQ(px[i * 3 + 2], 200);
    }
}

TEST(FrameCompositorTest, FullyTransparentOverlayLeavesFrameUnchanged) {
    auto src = MakeBgr(4, 4, 10, 20, 30);
    const auto overlay = MakeRgba(4, 4, 200, 100, 50, /*a=*/0);

    auto out = CompositeOverlay(src.frame, overlay);
    ASSERT_TRUE(out.has_value());
    const auto* px = out->planes[0].data;
    for (std::size_t i = 0; i < 16; ++i) {
        EXPECT_EQ(px[i * 3 + 0], 10);
        EXPECT_EQ(px[i * 3 + 1], 20);
        EXPECT_EQ(px[i * 3 + 2], 30);
    }
}

TEST(FrameCompositorTest, HalfAlphaBlendsHalfway) {
    auto src = MakeBgr(2, 2, /*b=*/0, /*g=*/0, /*r=*/0);
    const auto overlay = MakeRgba(2, 2, /*r=*/200, /*g=*/200, /*b=*/200, /*a=*/128);

    auto out = CompositeOverlay(src.frame, overlay);
    ASSERT_TRUE(out.has_value());
    const auto* px = out->planes[0].data;
    // out = 0*(127/255) + 200*(128/255) ≈ 100.
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_NEAR(px[i * 3 + 0], 100, 2);
        EXPECT_NEAR(px[i * 3 + 1], 100, 2);
        EXPECT_NEAR(px[i * 3 + 2], 100, 2);
    }
}

TEST(FrameCompositorTest, NonBgrSourceReturnsNullopt) {
    auto src = MakeBgr(4, 4, 1, 2, 3);
    src.frame.format = sst::common::PixelFormat::NV12;  // unsupported
    EXPECT_FALSE(CompositeOverlay(src.frame, MakeRgba(4, 4, 1, 1, 1, 255)).has_value());
}

TEST(FrameCompositorTest, EmptyOverlayReturnsNullopt) {
    auto src = MakeBgr(4, 4, 1, 2, 3);
    RgbaImage empty;  // 0x0, no pixels
    EXPECT_FALSE(CompositeOverlay(src.frame, empty).has_value());
}

TEST(FrameCompositorTest, MismatchedAspectIsLetterboxedAndCentered) {
    // Wide 4x2 frame (2:1), square 2x2 overlay (1:1): aspect-preserving scale =
    // min(4/2, 2/2) = 1, so the 2x2 overlay is drawn centered at x in [1,3) with
    // pillarbox margins at x=0 and x=3.
    auto src = MakeBgr(4, 2, /*b=*/0, /*g=*/0, /*r=*/0);
    const auto overlay = MakeRgba(2, 2, /*r=*/255, /*g=*/255, /*b=*/255, /*a=*/255);

    auto out = CompositeOverlay(src.frame, overlay);
    ASSERT_TRUE(out.has_value());
    const auto* px = out->planes[0].data;
    auto at = [&](std::uint32_t x, std::uint32_t y, std::uint32_t ch) {
        return px[(static_cast<std::size_t>(y) * 4 + x) * 3 + ch];
    };
    EXPECT_EQ(at(0, 0, 0), 0);    // left margin — untouched black
    EXPECT_EQ(at(3, 0, 0), 0);    // right margin — untouched black
    EXPECT_EQ(at(1, 0, 0), 255);  // drawn region — white
    EXPECT_EQ(at(2, 1, 0), 255);  // drawn region — white
}

}  // namespace
