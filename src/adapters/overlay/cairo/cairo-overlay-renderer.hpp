#pragma once

#include <cstdint>

#include "app/overlay/ports/overlay-renderer.hpp"
#include "domain/overlay/models/render-scene.hpp"

namespace sst::adapters::overlay {

// Cairo (shapes / rounded-rects / alpha) + Pango (text layout) RGBA renderer.
// Draws onto an offscreen ARGB32 image surface — no GPU, no X — then converts to
// straight-alpha RGBA8888. This is the best shot at pixel-parity with the app's
// Flutter overlay (KTD5). Stateless and reentrant: each Render() owns its
// surface.
class CairoOverlayRenderer final : public sst::overlay::IOverlayRenderer {
   public:
    CairoOverlayRenderer() = default;

    auto Render(const sst::overlay::RenderScene& scene, std::uint32_t out_width,
                std::uint32_t out_height) -> sst::overlay::RgbaImage override;
};

}  // namespace sst::adapters::overlay
