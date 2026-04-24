#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "app/config/ports/config-reader.hpp"
#include "domain/config/models/calibration.hpp"
#include "domain/config/models/config-data.hpp"
#include "domain/config/models/device.hpp"

namespace sst::config::app {

using sst::config::CalibrationConfig;
using sst::config::ConfigData;
using sst::config::DeviceConfig;
using sst::config::IConfigFileReaderAdapter;

class ConfigLoader {
   public:
    ConfigLoader(std::string root_path, std::string file_type,
                 std::optional<std::uint32_t> user_id = std::nullopt);

    auto get() -> ConfigData;

   private:
    std::optional<std::uint32_t> user_id_;
    std::string root_path_;
    std::string file_type_;
    std::unique_ptr<IConfigFileReaderAdapter<DeviceConfig>> deviceAdapter_;
    std::unique_ptr<IConfigFileReaderAdapter<CalibrationConfig>> calibrationAdapter_;
};

}  // namespace sst::config::app
