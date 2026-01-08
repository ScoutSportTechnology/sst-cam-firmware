#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "config/domain/calibration_config.hpp"
#include "config/domain/config_data.hpp"
#include "config/domain/device_config.hpp"
#include "config/domain/profile_config.hpp"
#include "config/domain/storage_config.hpp"
#include "config/domain/stream_config.hpp"
#include "config/domain/users_config.hpp"
#include "config/ports/config_file_adapter.hpp"

namespace sst::config::app {

using sst::config::domain::CalibrationConfig;
using sst::config::domain::ConfigData;
using sst::config::domain::DeviceConfig;
using sst::config::domain::ProfileConfig;
using sst::config::domain::StorageConfig;
using sst::config::domain::StreamConfig;
using sst::config::domain::UsersConfig;

using sst::config::ports::IConfigFileAdapter;

using sst::config::ports::IConfigFileAdapter;

class ConfigLoader {
   public:
    ConfigLoader(std::string root_path, std::string file_type,
                 std::optional<std::uint32_t> user_id = std::nullopt);

    auto get() -> ConfigData;

   private:
    std::optional<std::uint32_t> user_id_;
    std::string root_path_;
    std::string file_type_;
    std::unique_ptr<IConfigFileAdapter<UsersConfig>> usersAdapter_;
    std::unique_ptr<IConfigFileAdapter<ProfileConfig>> profileAdapter_;
    std::unique_ptr<IConfigFileAdapter<DeviceConfig>> deviceAdapter_;
    std::unique_ptr<IConfigFileAdapter<StorageConfig>> storageAdapter_;
    std::unique_ptr<IConfigFileAdapter<StreamConfig>> streamAdapter_;
    std::unique_ptr<IConfigFileAdapter<CalibrationConfig>> calibrationAdapter_;
};

}  // namespace sst::config::app
