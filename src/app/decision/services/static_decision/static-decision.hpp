#pragma once

#include <optional>
#include <vector>

#include "app/decision/ports/decision.hpp"
#include "domain/decision/models/camera-choice.hpp"
#include "domain/processing/models/frame-bundle.hpp"

namespace sst::decision {

// No-AI decision: always selects camera 0, full frame. The deterministic
// stand-in for the eventual AI/physics/decision stack — same IDecision port,
// no skeletons. Ignores camera 1 entirely; the second camera still runs (so the
// real dual-camera architecture is exercised and raw dual capture works), its
// bundles simply age out unchosen.
class StaticDecision final : public IDecision {
   public:
    // Returns { camera_index = 0, full-frame crop } when camera 0 has a frame.
    // Returns std::nullopt when camera 0 is absent this tick or its geometry is
    // degenerate (zero width/height) — defined, logged, never a crash.
    auto Decide(const std::vector<std::optional<sst::processing::FrameBundle>>& cameras)
        -> std::optional<CameraChoice> override;
};

}  // namespace sst::decision
