#pragma once

#include "domain/config/models/calibration.hpp"
#include "domain/config/models/device.hpp"
#include "domain/config/models/storage.hpp"
#include "domain/config/models/stream.hpp"

namespace sst::config::domain {
struct ConfigData {
    sst::config::domain::CalibrationData calibration;
    sst::config::domain::DeviceData device;
    sst::config::domain::StorageData storage;
    sst::config::domain::StreamData stream;
};
}  // namespace sst::config::domain
