#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

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

template <typename T>
static auto HasUser(const T& users, std::uint32_t user_id_to_search) -> bool {
    return std::any_of(users.begin(), users.end(),
                       [&](const auto& user) { return user.id == user_id_to_search; });
}

template <typename T>
static auto RetrieveUserById(const T& users, std::uint32_t user_id) -> const
    typename T::value_type* {
    auto userIterator = std::find_if(users.begin(), users.end(),
                                     [&](const auto& user) { return user.id == user_id; });
    return (userIterator == users.end()) ? nullptr : &*userIterator;
}

template <typename FileModel, typename DataModel>
static auto GetData(const FileModel& file, const std::optional<std::uint32_t>& userId)
    -> DataModel {
    DataModel out = file.default_data;

    if (!userId) {
        return out;
    }

    const auto* entry = RetrieveUserById(file.users, *userId);
    if (!entry) {
        return out;
    }

    apply_patch(out, entry->user_data);
    return out;
}

class ConfigLoader {
   public:
    ConfigLoader(IConfigFileAdapter<UsersConfig>& usersAdapter,
                 IConfigFileAdapter<ProfileConfig>& profileAdapter,
                 IConfigFileAdapter<DeviceConfig>& deviceAdapter,
                 IConfigFileAdapter<StorageConfig>& storageAdapter,
                 IConfigFileAdapter<StreamConfig>& streamAdapter,
                 IConfigFileAdapter<CalibrationConfig>& calibrationAdapter,
                 std::optional<std::uint32_t> userId = std::nullopt)
        : usersAdapter_(usersAdapter),
          profileAdapter_(profileAdapter),
          deviceAdapter_(deviceAdapter),
          storageAdapter_(storageAdapter),
          streamAdapter_(streamAdapter),
          calibrationAdapter_(calibrationAdapter),
          userId_(userId) {}

    auto get() -> ConfigData;

   private:
    IConfigFileAdapter<UsersConfig>& usersAdapter_;
    IConfigFileAdapter<ProfileConfig>& profileAdapter_;
    IConfigFileAdapter<DeviceConfig>& deviceAdapter_;
    IConfigFileAdapter<StorageConfig>& storageAdapter_;
    IConfigFileAdapter<StreamConfig>& streamAdapter_;
    IConfigFileAdapter<CalibrationConfig>& calibrationAdapter_;
    std::optional<std::uint32_t> userId_;
};

}  // namespace sst::config::app
