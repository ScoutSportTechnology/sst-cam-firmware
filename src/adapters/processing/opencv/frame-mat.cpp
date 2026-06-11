#include "adapters/processing/opencv/frame-mat.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <opencv2/core.hpp>
#include <optional>
#include <vector>

#include "domain/capture/models/frame.hpp"
#include "domain/common/models/memory-type.hpp"
#include "domain/common/models/pixel-format.hpp"
#include "domain/common/models/timestamp.hpp"

namespace sst::adapters::processing {

auto WrapNv12(const sst::capture::Frame& f) -> std::optional<cv::Mat> {
    if (f.format != sst::common::PixelFormat::NV12) {
        return std::nullopt;
    }
    if (f.planes.size() != 2) {
        return std::nullopt;
    }
    const auto w = f.geometry.width;
    const auto h = f.geometry.height;
    if (w == 0 || h == 0 || (w % 2) != 0 || (h % 2) != 0) {
        return std::nullopt;
    }

    const auto& y = f.planes[0];
    const auto& uv = f.planes[1];
    if (y.data == nullptr || uv.data == nullptr) {
        return std::nullopt;
    }

    const bool contiguous = (y.stride == w) && (uv.stride == w) && (y.data + y.size == uv.data);

    const int total_rows = static_cast<int>(h) + static_cast<int>(h) / 2;

    if (contiguous) {
        // Non-owning Mat over the input memory.
        return cv::Mat(total_rows, static_cast<int>(w), CV_8UC1, const_cast<std::uint8_t*>(y.data),
                       y.stride);
    }

    // Fallback: allocate compact (H*3/2 x W) CV_8UC1 and row-copy.
    cv::Mat out(total_rows, static_cast<int>(w), CV_8UC1);
    for (std::uint32_t row = 0; row < h; ++row) {
        std::memcpy(out.ptr(static_cast<int>(row)), y.data + row * y.stride, w);
    }
    for (std::uint32_t row = 0; row < h / 2; ++row) {
        std::memcpy(out.ptr(static_cast<int>(h + row)), uv.data + row * uv.stride, w);
    }
    return out;
}

auto MakeOwnedFrame(const cv::Mat& mat, sst::common::PixelFormat fmt, std::uint64_t frame_id,
                    sst::common::Timestamp ts) -> sst::capture::Frame {
    assert(mat.depth() == CV_8U && "MakeOwnedFrame requires CV_8U mat");
    assert(mat.cols > 0 && mat.rows > 0);

    const auto cols = static_cast<std::uint32_t>(mat.cols);
    const auto rows = static_cast<std::uint32_t>(mat.rows);
    const auto channels = static_cast<std::uint32_t>(mat.channels());
    const std::size_t row_bytes = static_cast<std::size_t>(cols) * channels;
    const std::size_t total = static_cast<std::size_t>(rows) * row_bytes;

    auto buf = std::make_shared<std::vector<std::uint8_t>>(total);
    for (std::uint32_t r = 0; r < rows; ++r) {
        std::memcpy(buf->data() + r * row_bytes, mat.ptr(static_cast<int>(r)), row_bytes);
    }

    sst::capture::Frame out;
    out.frame_id = frame_id;
    out.format = fmt;
    out.memory = sst::common::MemoryType::CPU;
    out.geometry = {cols, rows};
    out.captured_at = ts;
    out.planes.push_back(sst::capture::FramePlane{
        .stride = static_cast<std::uint32_t>(row_bytes),
        .data = buf->data(),
        .size = total,
    });
    out.owner = std::shared_ptr<void>(buf);
    return out;
}

}  // namespace sst::adapters::processing
