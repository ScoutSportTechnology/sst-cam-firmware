#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "domain/capture/models/frame.hpp"
#include "domain/common/models/memory-type.hpp"
#include "domain/common/models/pixel-format.hpp"
#include "domain/common/models/timestamp.hpp"

namespace sst::tests::processing {

// Builds a 2-plane NV12 Frame whose `owner` pins a vector<uint8_t>.
// Y plane filled with `y`, UV plane filled with alternating (u, v) pairs.
// `stride == 0` means stride = w (no row padding).
inline auto MakeNv12Frame(std::uint32_t w, std::uint32_t h, std::uint8_t y, std::uint8_t u,
                          std::uint8_t v, std::uint32_t stride = 0, std::uint64_t frame_id = 1)
    -> sst::capture::Frame {
    if (stride == 0) {
        stride = w;
    }
    const std::size_t y_size = static_cast<std::size_t>(stride) * h;
    const std::size_t uv_size = static_cast<std::size_t>(stride) * (h / 2);
    auto buf = std::make_shared<std::vector<std::uint8_t>>(y_size + uv_size);

    for (std::uint32_t row = 0; row < h; ++row) {
        for (std::uint32_t col = 0; col < w; ++col) {
            (*buf)[row * stride + col] = y;
        }
    }
    for (std::uint32_t row = 0; row < h / 2; ++row) {
        for (std::uint32_t col = 0; col < w; col += 2) {
            (*buf)[y_size + row * stride + col + 0] = u;
            (*buf)[y_size + row * stride + col + 1] = v;
        }
    }

    sst::capture::Frame f;
    f.frame_id = frame_id;
    f.format = sst::common::PixelFormat::NV12;
    f.memory = sst::common::MemoryType::CPU;
    f.geometry = {w, h};
    f.captured_at = sst::common::Timestamp{std::chrono::milliseconds{1000}};
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

// Builds a 1-plane BGR8 Frame (stride == w*3, no padding).
inline auto MakeBgr8Frame(std::uint32_t w, std::uint32_t h, std::uint8_t b, std::uint8_t g,
                          std::uint8_t r) -> sst::capture::Frame {
    auto buf = std::make_shared<std::vector<std::uint8_t>>(static_cast<std::size_t>(w) * h * 3);
    for (std::size_t i = 0; i < buf->size(); i += 3) {
        (*buf)[i + 0] = b;
        (*buf)[i + 1] = g;
        (*buf)[i + 2] = r;
    }
    sst::capture::Frame f;
    f.frame_id = 1;
    f.format = sst::common::PixelFormat::BGR8;
    f.memory = sst::common::MemoryType::CPU;
    f.geometry = {w, h};
    f.captured_at = sst::common::Timestamp{std::chrono::milliseconds{1000}};
    f.planes.push_back(sst::capture::FramePlane{
        .stride = w * 3,
        .data = buf->data(),
        .size = buf->size(),
    });
    f.owner = std::shared_ptr<void>(buf);
    return f;
}

}  // namespace sst::tests::processing
