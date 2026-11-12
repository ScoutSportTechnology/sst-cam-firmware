#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "bluetooth.pb.h"

namespace sst::adapters::control {

using CameraStatuses = std::array<sst_cam::CameraHealth, 2>;

auto RunCameraProbeChild(const std::vector<std::string>& argv,
                         std::chrono::seconds timeout) -> sst_cam::CameraHealth;
auto ProbeCameras(std::array<std::int64_t, 2>* durations_ms = nullptr) -> CameraStatuses;
auto RunCameraProbe(int index) -> int;

}  // namespace sst::adapters::control
