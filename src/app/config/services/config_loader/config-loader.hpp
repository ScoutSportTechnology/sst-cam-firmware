#pragma once

#include <memory>
#include <string>

#include "app/config/ports/config-reader.hpp"
#include "domain/config/models/calibration.hpp"
#include "domain/config/models/config-data.hpp"
#include "domain/config/models/device.hpp"
#include "domain/config/models/storage.hpp"

namespace sst::config::app {

using sst::config::CalibrationData;
using sst::config::ConfigData;
using sst::config::DeviceData;
using sst::config::IConfigFileReaderAdapter;
using sst::config::StorageData;

class ConfigLoader {
   public:
    ConfigLoader(std::string root_path, std::string file_type);

    auto get() -> ConfigData;

   private:
    std::string root_path_;
    std::string file_type_;
    std::unique_ptr<IConfigFileReaderAdapter<DeviceData>> deviceAdapter_;
    std::unique_ptr<IConfigFileReaderAdapter<CalibrationData>> calibrationAdapter_;
    std::unique_ptr<IConfigFileReaderAdapter<StorageData>> storageAdapter_;
};

}  // namespace sst::config::app
