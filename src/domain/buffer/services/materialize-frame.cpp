#include "domain/buffer/services/materialize-frame.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "domain/capture/models/frame.hpp"

namespace sst::buffer {

auto MaterializeFrame(const sst::capture::Frame& src) -> sst::capture::Frame {
    std::size_t total = 0;
    for (const auto& p : src.planes) {
        total += p.size;
    }

    auto buf = std::make_shared<std::vector<std::uint8_t>>(total);

    sst::capture::Frame out;
    out.frame_id = src.frame_id;
    out.format = src.format;
    out.memory = src.memory;
    out.geometry = src.geometry;
    out.captured_at = src.captured_at;
    out.planes.reserve(src.planes.size());

    std::size_t offset = 0;
    for (const auto& p : src.planes) {
        if (p.size > 0 && p.data != nullptr) {
            std::memcpy(buf->data() + offset, p.data, p.size);
        }
        out.planes.push_back(sst::capture::FramePlane{
            .stride = p.stride,
            .data = buf->data() + offset,
            .size = p.size,
        });
        offset += p.size;
    }

    out.owner = std::shared_ptr<void>(buf);
    return out;
}

}  // namespace sst::buffer
