#pragma once

#include "config/domain/calibration_config.hpp"
#include "config/domain/device_config.hpp"
#include "config/domain/storage_config.hpp"
#include "config/domain/stream_config.hpp"

namespace sst::config::domain {
struct ConfigData {
    sst::config::domain::CalibrationData calibration;
    sst::config::domain::DeviceData device;
    sst::config::domain::StorageData storage;
    sst::config::domain::StreamData stream;
};
}  // namespace sst::config::domain
