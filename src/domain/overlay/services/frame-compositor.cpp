#include "domain/overlay/services/frame-compositor.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace sst::overlay {

namespace {
constexpr std::uint32_t kBgrBytesPerPixel = 3;
constexpr std::uint32_t kRgbaBytesPerPixel = 4;
}  // namespace

auto CompositeOverlay(const sst::capture::Frame& source, const RgbaImage& overlay)
    -> std::optional<sst::capture::Frame> {
    // Only BGR8 single-plane frames are supported (the postprocessor's output).
    if (source.format != sst::common::PixelFormat::BGR8 || source.planes.empty() ||
        source.planes[0].data == nullptr) {
        return std::nullopt;
    }
    const std::uint32_t fw = source.geometry.width;
    const std::uint32_t fh = source.geometry.height;
    if (fw == 0 || fh == 0) {
        return std::nullopt;
    }
    if (overlay.width == 0 || overlay.height == 0 || overlay.pixels.empty()) {
        return std::nullopt;  // nothing to draw
    }
    // The blend loop indexes overlay.pixels via stride/width; a malformed overlay
    // (stride narrower than a full RGBA row, or a buffer shorter than
    // stride*height) would read out of bounds. Reject it rather than trust the
    // caller. Max read offset is (height-1)*stride + (width-1)*4 + 3, which is
    // < stride*height once both conditions below hold.
    if (overlay.stride < static_cast<std::size_t>(overlay.width) * kRgbaBytesPerPixel ||
        overlay.pixels.size() < static_cast<std::size_t>(overlay.stride) * overlay.height) {
        return std::nullopt;
    }

    const std::uint32_t src_stride = source.planes[0].stride;
    const auto* src = source.planes[0].data;

    // Copy the source BGR bytes into an owned buffer we can blend into.
    auto out_bytes = std::make_shared<std::vector<std::uint8_t>>(
        src, src + static_cast<std::size_t>(src_stride) * fh);

    // Uniform aspect-preserving scale + centered letterbox.
    const double scale = std::min(static_cast<double>(fw) / overlay.width,
                                  static_cast<double>(fh) / overlay.height);
    const auto draw_w = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(overlay.width * scale));
    const auto draw_h =
        std::max<std::uint32_t>(1, static_cast<std::uint32_t>(overlay.height * scale));
    const std::uint32_t off_x = (fw - std::min(fw, draw_w)) / 2;
    const std::uint32_t off_y = (fh - std::min(fh, draw_h)) / 2;

    auto& dst = *out_bytes;
    for (std::uint32_t dy = 0; dy < draw_h && (off_y + dy) < fh; ++dy) {
        // Nearest-neighbor sample row in the overlay.
        const auto sy = std::min(overlay.height - 1, static_cast<std::uint32_t>(dy / scale));
        for (std::uint32_t dx = 0; dx < draw_w && (off_x + dx) < fw; ++dx) {
            const auto sx = std::min(overlay.width - 1, static_cast<std::uint32_t>(dx / scale));
            const std::size_t op =
                static_cast<std::size_t>(sy) * overlay.stride + static_cast<std::size_t>(sx) * kRgbaBytesPerPixel;
            const std::uint8_t r = overlay.pixels[op + 0];
            const std::uint8_t g = overlay.pixels[op + 1];
            const std::uint8_t b = overlay.pixels[op + 2];
            const std::uint8_t a = overlay.pixels[op + 3];
            if (a == 0) {
                continue;  // fully transparent — leave the video pixel
            }
            const std::size_t dp = static_cast<std::size_t>(off_y + dy) * src_stride +
                                   static_cast<std::size_t>(off_x + dx) * kBgrBytesPerPixel;
            // Source byte order is BGR; overlay is RGBA. Blend: out = src*(1-a)+ov*a.
            const std::uint32_t inv = 255U - a;
            dst[dp + 0] = static_cast<std::uint8_t>((dst[dp + 0] * inv + b * a) / 255U);  // B
            dst[dp + 1] = static_cast<std::uint8_t>((dst[dp + 1] * inv + g * a) / 255U);  // G
            dst[dp + 2] = static_cast<std::uint8_t>((dst[dp + 2] * inv + r * a) / 255U);  // R
        }
    }

    sst::capture::Frame out = source;  // copies metadata (id, geometry, timestamp)
    out.memory = sst::common::MemoryType::CPU;
    const auto* base = out_bytes->data();
    out.planes = {sst::capture::FramePlane{
        .stride = src_stride, .data = base, .size = out_bytes->size()}};
    out.owner = out_bytes;  // keep the blended bytes alive for the frame's lifetime
    return out;
}

}  // namespace sst::overlay
