#pragma once

#include "domain/config/models/calibration.hpp"
#include "domain/config/models/device.hpp"
#include "domain/config/models/storage.hpp"

namespace sst::config {
struct ConfigData {
    sst::config::CalibrationData calibration;
    sst::config::DeviceData device;
    sst::config::StorageData storage;
};
}  // namespace sst::config
