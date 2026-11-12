#pragma once

#include <array>
#include <string_view>

#include "bluetooth.pb.h"

namespace sst::adapters::control {

using CameraStatuses = std::array<sst_cam::CameraHealth, 2>;

auto DecodeCameraProbe(std::string_view output, bool exited_ok) -> CameraStatuses;
auto ProbeCameras() -> CameraStatuses;
auto RunCameraProbe(int index) -> int;

}  // namespace sst::adapters::control
