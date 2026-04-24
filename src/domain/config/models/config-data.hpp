#pragma once

#include "domain/config/models/calibration.hpp"
#include "domain/config/models/device.hpp"

namespace sst::config {
struct ConfigData {
    sst::config::CalibrationData calibration;
    sst::config::DeviceData device;
};
}  // namespace sst::config
