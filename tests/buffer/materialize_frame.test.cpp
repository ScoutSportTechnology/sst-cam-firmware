#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "domain/buffer/services/materialize-frame.hpp"
#include "domain/capture/models/frame.hpp"
#include "domain/common/models/memory-type.hpp"
#include "domain/common/models/pixel-format.hpp"

namespace sst::buffer {

namespace {

// Builds a 1-plane BGR8 Frame whose owner pins a vector<uint8_t>.
auto MakeBgr8Frame(std::uint32_t w, std::uint32_t h, std::uint8_t b, std::uint8_t g, std::uint8_t r)
    -> sst::capture::Frame {
    auto buf = std::make_shared<std::vector<std::uint8_t>>(static_cast<std::size_t>(w) * h * 3);
    for (std::size_t i = 0; i < buf->size(); i += 3) {
        (*buf)[i + 0] = b;
        (*buf)[i + 1] = g;
        (*buf)[i + 2] = r;
    }
    sst::capture::Frame f;
    f.frame_id = 42;
    f.format = sst::common::PixelFormat::BGR8;
    f.memory = sst::common::MemoryType::CPU;
    f.geometry = {w, h};
    f.captured_at = sst::common::Timestamp{std::chrono::milliseconds{12345}};
    f.planes.push_back(sst::capture::FramePlane{
        .stride = w * 3,
        .data = buf->data(),
        .size = buf->size(),
    });
    f.owner = std::shared_ptr<void>(buf);
    return f;
}

// Builds a 2-plane NV12 Frame with optional row-stride padding (>= width).
// Y plane filled with `y`, UV plane filled with alternating u, v.
auto MakeNv12Frame(std::uint32_t w, std::uint32_t h, std::uint8_t y, std::uint8_t u, std::uint8_t v,
                   std::uint32_t stride = 0) -> sst::capture::Frame {
    if (stride == 0) {
        stride = w;
    }
    const std::size_t y_size = static_cast<std::size_t>(stride) * h;
    const std::size_t uv_size = static_cast<std::size_t>(stride) * (h / 2);
    auto buf = std::make_shared<std::vector<std::uint8_t>>(y_size + uv_size);

    // Fill Y plane (only the first `w` bytes of each row are meaningful).
    for (std::uint32_t row = 0; row < h; ++row) {
        for (std::uint32_t col = 0; col < w; ++col) {
            (*buf)[row * stride + col] = y;
        }
    }
    // Fill UV plane (interleaved, half-height).
    for (std::uint32_t row = 0; row < h / 2; ++row) {
        for (std::uint32_t col = 0; col < w; col += 2) {
            (*buf)[y_size + row * stride + col + 0] = u;
            (*buf)[y_size + row * stride + col + 1] = v;
        }
    }

    sst::capture::Frame f;
    f.frame_id = 7;
    f.format = sst::common::PixelFormat::NV12;
    f.memory = sst::common::MemoryType::CPU;
    f.geometry = {w, h};
    f.captured_at = sst::common::Timestamp{std::chrono::milliseconds{99}};
    f.planes.push_back(sst::capture::FramePlane{
        .stride = stride,
        .data = buf->data(),
        .size = y_size,
    });
    f.planes.push_back(sst::capture::FramePlane{
        .stride = stride,
        .data = buf->data() + y_size,
        .size = uv_size,
    });
    f.owner = std::shared_ptr<void>(buf);
    return f;
}

}  // namespace

TEST(MaterializeFrameTest, RoundTripPixelsNv12) {
    auto src = MakeNv12Frame(64, 32, /*y=*/180, /*u=*/100, /*v=*/200);

    auto out = MaterializeFrame(src);

    ASSERT_EQ(out.planes.size(), 2U);
    ASSERT_EQ(out.planes[0].size, src.planes[0].size);
    ASSERT_EQ(out.planes[1].size, src.planes[1].size);
    EXPECT_EQ(0, std::memcmp(out.planes[0].data, src.planes[0].data, src.planes[0].size));
    EXPECT_EQ(0, std::memcmp(out.planes[1].data, src.planes[1].data, src.planes[1].size));
}

TEST(MaterializeFrameTest, RoundTripPixelsBgr8) {
    auto src = MakeBgr8Frame(32, 32, /*b=*/10, /*g=*/20, /*r=*/30);

    auto out = MaterializeFrame(src);

    ASSERT_EQ(out.planes.size(), 1U);
    ASSERT_EQ(out.planes[0].size, src.planes[0].size);
    EXPECT_EQ(0, std::memcmp(out.planes[0].data, src.planes[0].data, src.planes[0].size));
}

TEST(MaterializeFrameTest, OutputOwnsMemoryReleasesInputOwner) {
    auto out = [] {
        auto src = MakeBgr8Frame(16, 16, 1, 2, 3);
        return MaterializeFrame(src);
    }();  // src and its vector are dropped here.

    ASSERT_EQ(out.planes.size(), 1U);
    ASSERT_NE(out.owner, nullptr);
    // Reading every byte must not crash and must match the fill.
    for (std::size_t i = 0; i < out.planes[0].size; i += 3) {
        EXPECT_EQ(out.planes[0].data[i + 0], 1);
        EXPECT_EQ(out.planes[0].data[i + 1], 2);
        EXPECT_EQ(out.planes[0].data[i + 2], 3);
    }
}

TEST(MaterializeFrameTest, MultiPlaneStridesPreserved) {
    constexpr std::uint32_t kW = 32;
    constexpr std::uint32_t kH = 16;
    constexpr std::uint32_t kStride = kW + 16;
    auto src = MakeNv12Frame(kW, kH, 50, 60, 70, kStride);

    auto out = MaterializeFrame(src);

    ASSERT_EQ(out.planes.size(), 2U);
    EXPECT_EQ(out.planes[0].stride, kStride);
    EXPECT_EQ(out.planes[1].stride, kStride);
    EXPECT_EQ(out.geometry.width, kW);
    EXPECT_EQ(out.geometry.height, kH);

    // Pixel at (row, col) in Y plane must match input.
    for (std::uint32_t row = 0; row < kH; ++row) {
        for (std::uint32_t col = 0; col < kW; ++col) {
            EXPECT_EQ(out.planes[0].data[row * kStride + col], 50);
        }
    }
}

TEST(MaterializeFrameTest, MetadataPropagated) {
    auto src = MakeBgr8Frame(8, 8, 0, 0, 0);
    src.frame_id = 0xDEADBEEF;
    src.captured_at = sst::common::Timestamp{std::chrono::milliseconds{555}};

    auto out = MaterializeFrame(src);

    EXPECT_EQ(out.frame_id, 0xDEADBEEFU);
    EXPECT_EQ(out.format, sst::common::PixelFormat::BGR8);
    EXPECT_EQ(out.memory, sst::common::MemoryType::CPU);
    EXPECT_EQ(out.geometry.width, 8U);
    EXPECT_EQ(out.geometry.height, 8U);
    EXPECT_EQ(out.captured_at, src.captured_at);
}

TEST(MaterializeFrameTest, OriginalOwnerNotMutated) {
    auto src = MakeBgr8Frame(8, 8, 0, 0, 0);
    const auto before = src.owner.use_count();

    auto out = MaterializeFrame(src);

    EXPECT_EQ(src.owner.use_count(), before);
    EXPECT_NE(out.owner, src.owner);
}

}  // namespace sst::buffer
