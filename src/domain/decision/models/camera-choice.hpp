#pragma once

#include <cstdint>

#include "domain/processing/models/crop-rect.hpp"

namespace sst::decision {

// The output of the intelligence seam (IDecision): which camera's frame to
// present and which region of it to keep. `crop` is in the chosen camera's
// source-frame pixel coordinates (same CropRect contract the postprocessor
// consumes). For the no-AI demo this is always { camera 0, full frame }; the
// AI/physics/decision stack later produces dynamic values behind the same port.
struct CameraChoice {
    std::uint32_t camera_index{0};
    sst::processing::CropRect crop{};
};

}  // namespace sst::decision
