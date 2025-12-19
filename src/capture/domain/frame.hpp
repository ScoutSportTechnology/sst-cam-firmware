#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "common/domain/image_orientation.hpp"
#include "common/domain/memory_type.hpp"
#include "common/domain/pixel_format.hpp"
#include "common/domain/timestamp.hpp"

namespace sst::video::domain {
using sst::common::domain::ImageOrientation;
using sst::common::domain::MemoryType;
using sst::common::domain::PixelFormat;
using sst::common::domain::Timestamp;

struct FramePlane {
    std::uint32_t stride{0};
    void* data{nullptr};
    std::size_t size{0};
};

struct FrameMetadata {
    std::uint32_t width{0};
    std::uint32_t height{0};
};

struct Frame {
    std::uint64_t frame_id{0};
    PixelFormat format{PixelFormat::UNKNOWN};
    MemoryType memory{MemoryType::CPU};
    FrameMetadata metadata{};
    std::vector<FramePlane> planes;
    Timestamp captured_at;
    std::shared_ptr<void> owner{nullptr};
};

}  // namespace sst::video::domain
