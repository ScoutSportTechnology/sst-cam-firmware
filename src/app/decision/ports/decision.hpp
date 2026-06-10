#pragma once

#include <optional>
#include <vector>

#include "domain/decision/models/camera-choice.hpp"
#include "domain/processing/models/frame-bundle.hpp"

namespace sst::decision {

// The intelligence seam. Given the latest materialized bundle from each camera
// (indexed by camera_index; std::nullopt means that camera has no fresh frame
// this tick), choose which camera to present and the crop to apply.
//
// Returns std::nullopt when no usable frame is available — the caller skips the
// tick rather than postprocessing nothing. Implementations must be deterministic
// and must not pin the passed bundles past the call (the orchestrator owns them).
//
// `StaticDecision` is the no-AI implementation (always camera 0, full frame).
// The AI/physics/decision stack later implements the same port with dynamic
// camera selection and zoom/crop.
class IDecision {
   public:
    IDecision() = default;
    virtual ~IDecision() = default;

    IDecision(const IDecision&) = delete;
    auto operator=(const IDecision&) -> IDecision& = delete;
    IDecision(IDecision&&) = delete;
    auto operator=(IDecision&&) -> IDecision& = delete;

    virtual auto Decide(
        const std::vector<std::optional<sst::processing::FrameBundle>>& cameras)
        -> std::optional<CameraChoice> = 0;
};

}  // namespace sst::decision
