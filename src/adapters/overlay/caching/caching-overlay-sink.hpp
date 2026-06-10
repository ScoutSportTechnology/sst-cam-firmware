#pragma once

#include <mutex>
#include <optional>

#include "app/overlay/ports/overlay-frame-source.hpp"
#include "app/overlay/ports/overlay-sink.hpp"
#include "domain/overlay/models/render-scene.hpp"

namespace sst::adapters::overlay {

// Bridges the OverlayController to the pipeline for CPU-side compositing. The
// controller pushes freshly-rendered RGBA frames in (IOverlaySink, push-on-
// change); the pipeline consumer reads the latest one out (IOverlayFrameSource)
// and alpha-blends it onto each final frame.
//
// Supersedes GstOverlayCompositor for the demo: rather than composite inside a
// GStreamer bin upstream of a tee, we blend once in CPU on the BGR final frame
// before it fans out to recording/RTSP/RTMP, so all three carry identical
// pixels through a single, testable code path.
class CachingOverlaySink final : public sst::overlay::IOverlaySink,
                                 public sst::overlay::IOverlayFrameSource {
   public:
    auto PushFrame(const sst::overlay::RgbaImage& frame) -> void override {
        std::lock_guard lock(mtx_);
        latest_ = frame;
    }

    [[nodiscard]] auto LatestOverlay() const -> std::optional<sst::overlay::RgbaImage> override {
        std::lock_guard lock(mtx_);
        return latest_;
    }

   private:
    mutable std::mutex mtx_;
    std::optional<sst::overlay::RgbaImage> latest_;
};

}  // namespace sst::adapters::overlay
