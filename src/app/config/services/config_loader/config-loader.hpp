#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "app/config/ports/config-reader.hpp"
//#include "app/config/ports/config-writer.hpp"
#include "domain/config/models/config-data.hpp"
#include "domain/config/models/device.hpp"
#include "domain/config/models/profile.hpp"
#include "domain/config/models/storage.hpp"
#include "domain/config/models/stream.hpp"
#include "domain/config/models/users.hpp"

namespace sst::config::app {

using sst::config::CalibrationConfig;
using sst::config::ConfigData;
using sst::config::DeviceConfig;
using sst::config::ProfileConfig;
using sst::config::StorageConfig;
using sst::config::StreamConfig;
using sst::config::UsersConfig;

using sst::config::IConfigFileReaderAdapter;
// using sst::config::IConfigFileWriterAdapter;

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
