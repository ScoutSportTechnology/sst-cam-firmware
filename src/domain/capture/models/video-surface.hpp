#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>

#include "domain/capture/models/frame.hpp"
#include "domain/common/models/pixel-format.hpp"

namespace sst::capture {

struct VideoSurface {
    std::shared_ptr<void> owner;
    std::shared_future<void> producer_fence;
    std::uint16_t camera_index{0};
    std::chrono::nanoseconds capture_pts{0};
    std::chrono::nanoseconds raw_pts{0};
    std::uint64_t clock_epoch{0};
    bool discontinuity{false};
    FrameGeometry geometry{};
    sst::common::PixelFormat format{sst::common::PixelFormat::NV12};
    std::size_t bytes{0};

    auto WaitForProducer() const -> void {
        if (producer_fence.valid()) {
            producer_fence.wait();
        }
    }
};

}  // namespace sst::capture
