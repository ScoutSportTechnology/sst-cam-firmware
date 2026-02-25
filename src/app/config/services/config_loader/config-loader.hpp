#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "app/config/ports/config-reader.hpp"
#include "app/config/ports/config-writer.hpp"
#include "domain/config/models//config_data.hpp"
#include "domain/config/models/config_data.hpp"
#include "domain/config/models/device.hpp"
#include "domain/config/models/profile.hpp"
#include "domain/config/models/storage.hpp"
#include "domain/config/models/stream.hpp"
#include "domain/config/models/users.hpp"

namespace sst::config::app {

using sst::config::domain::CalibrationConfig;
using sst::config::domain::ConfigData;
using sst::config::domain::DeviceConfig;
using sst::config::domain::ProfileConfig;
using sst::config::domain::StorageConfig;
using sst::config::domain::StreamConfig;
using sst::config::domain::UsersConfig;

using sst::config::ports::IConfigFileReaderAdapter;

using sst::config::ports::IConfigFileReaderAdapter;

class ConfigLoader {
   public:
    ConfigLoader(std::string root_path, std::string file_type,
                 std::optional<std::uint32_t> user_id = std::nullopt);

    auto get() -> ConfigData;

   private:
    std::optional<std::uint32_t> user_id_;
    std::string root_path_;
    std::string file_type_;
    std::unique_ptr<IConfigFileReaderAdapter<UsersConfig>> usersAdapter_;
    std::unique_ptr<IConfigFileReaderAdapter<ProfileConfig>> profileAdapter_;
    std::unique_ptr<IConfigFileReaderAdapter<DeviceConfig>> deviceAdapter_;
    std::unique_ptr<IConfigFileReaderAdapter<StorageConfig>> storageAdapter_;
    std::unique_ptr<IConfigFileReaderAdapter<StreamConfig>> streamAdapter_;
    std::unique_ptr<IConfigFileReaderAdapter<CalibrationConfig>> calibrationAdapter_;
};

}  // namespace sst::config::app
