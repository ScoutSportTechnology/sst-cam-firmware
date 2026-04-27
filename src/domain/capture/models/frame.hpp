#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "domain/common/models/memory-type.hpp"
#include "domain/common/models/pixel-format.hpp"
#include "domain/common/models/timestamp.hpp"

namespace sst::capture {
using sst::common::MemoryType;
using sst::common::PixelFormat;
using sst::common::Timestamp;

struct FramePlane {
    std::uint32_t stride{0};
    const uint8_t* data{nullptr};
    std::size_t size{0};
};

struct FrameGeometry {
    std::uint32_t width{0};
    std::uint32_t height{0};
};

struct Frame {
    std::uint64_t frame_id{0};
    PixelFormat format{PixelFormat::NV12};
    MemoryType memory{MemoryType::CPU};
    FrameGeometry geometry{};
    std::vector<FramePlane> planes;
    Timestamp captured_at;
    std::shared_ptr<void> owner{nullptr};
};

}  // namespace sst::capture