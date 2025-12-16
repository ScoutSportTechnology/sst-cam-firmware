#include "config/app/config_loader.hpp"

namespace sst::config::app {

using sst::config::domain::CalibrationData;
using sst::config::domain::ConfigData;
using sst::config::domain::DeviceData;
using sst::config::domain::ProfileData;
using sst::config::domain::StorageData;
using sst::config::domain::StreamData;

using sst::config::ports::ConfigReturn;

auto ConfigLoader::get() -> ConfigData {
    const auto usersConfig = usersAdapter_.load();
    const auto profileConfig = profileAdapter_.load();
    const auto deviceConfig = deviceAdapter_.load();
    const auto storageConfig = storageAdapter_.load();
    const auto streamConfig = streamAdapter_.load();
    const auto calibrationConfig = calibrationAdapter_.load();

    if (!usersConfig.success || !profileConfig.success || !deviceConfig.success ||
        !storageConfig.success || !streamConfig.success || !calibrationConfig.success) {
        spdlog::error("One or more config files failed to load.");
        throw std::runtime_error("failed to load configuration files");
    }

    const bool user_ok = userId_.has_value() && !usersConfig.data.users.empty() &&
                         HasUser(usersConfig.data.users, *userId_);

    ConfigData out{};

    if (!user_ok) {
        out.device = deviceConfig.data.default_data;
        out.storage = storageConfig.data.default_data;
        out.stream = streamConfig.data.default_data;
        out.calibration = calibrationConfig.data.default_data;
        return out;
    }

    const ProfileData profileData =
        GetData<ProfileConfig, ProfileData>(profileConfig.data, userId_);

    const bool device_modified = profileData.device.value_or(false);
    const bool storage_modified = profileData.storage.value_or(false);
    const bool stream_modified = profileData.stream.value_or(false);
    const bool calibration_modified = profileData.calibration.value_or(false);

    out.device = device_modified ? GetData<DeviceConfig, DeviceData>(deviceConfig.data, userId_)
                                 : deviceConfig.data.default_data;

    out.storage = storage_modified
                      ? GetData<StorageConfig, StorageData>(storageConfig.data, userId_)
                      : storageConfig.data.default_data;

    out.stream = stream_modified ? GetData<StreamConfig, StreamData>(streamConfig.data, userId_)
                                 : streamConfig.data.default_data;

    out.calibration = calibration_modified ? GetData<CalibrationConfig, CalibrationData>(
                                                 calibrationConfig.data, userId_)
                                           : calibrationConfig.data.default_data;

    return out;
}

}  // namespace sst::config::app
