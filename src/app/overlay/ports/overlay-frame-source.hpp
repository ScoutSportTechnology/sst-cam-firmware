#pragma once

#include <optional>

#include "domain/overlay/models/render-scene.hpp"

namespace sst::overlay {

// Read side of the overlay handoff to the pipeline. The OverlayController pushes
// freshly-rendered RGBA frames into an IOverlaySink (push-on-change); the
// pipeline consumer reads the latest one through this port and composites it
// onto each final frame. std::nullopt means no overlay has been produced yet.
class IOverlayFrameSource {
   public:
    virtual ~IOverlayFrameSource() = default;

    [[nodiscard]] virtual auto LatestOverlay() const -> std::optional<RgbaImage> = 0;
};

}  // namespace sst::overlay
