#include "config/app/config_loader.hpp"

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

#include "config/adapters/json/json_adapter.hpp"
#include "config/domain/calibration_config.hpp"
#include "config/domain/device_config.hpp"
#include "config/domain/formatter/config_data.fmt.hpp"
#include "config/domain/profile_config.hpp"
#include "config/domain/storage_config.hpp"
#include "config/domain/stream_config.hpp"
#include "config/domain/users_config.hpp"

namespace fs = std::filesystem;

template <typename T>
auto SetAdapter(const std::string& path_str) -> sst::config::adapters::JsonAdapter<T> {
    fs::path path = fs::path{SST_REPO_ROOT_DIR} / path_str;
    return sst::config::adapters::JsonAdapter<T>(path);
}

template <typename T>
auto LogObject(const T& object, std::string_view label = "Object") -> void {
    spdlog::info("{}:\n{}", label, object);
}

static constexpr const char* path_user = "tests/config/config_files/users.json";
static constexpr const char* path_profile = "tests/config/config_files/profile.json";
static constexpr const char* path_device = "tests/config/config_files/device.json";
static constexpr const char* path_calibration = "tests/config/config_files/calibration.json";
static constexpr const char* path_storage = "tests/config/config_files/storage.json";
static constexpr const char* path_stream = "tests/config/config_files/stream.json";

TEST(ConfigLoader, ConfigData_Defaults) {
    using sst::config::adapters::JsonAdapter;
    using sst::config::app::ConfigLoader;
    using sst::config::domain::CalibrationConfig;
    using sst::config::domain::DeviceConfig;
    using sst::config::domain::ProfileConfig;
    using sst::config::domain::StorageConfig;
    using sst::config::domain::StreamConfig;
    using sst::config::domain::UsersConfig;

    JsonAdapter<UsersConfig> usersAdapter = SetAdapter<UsersConfig>(path_user);
    JsonAdapter<ProfileConfig> profileAdapter = SetAdapter<ProfileConfig>(path_profile);
    JsonAdapter<DeviceConfig> deviceAdapter = SetAdapter<DeviceConfig>(path_device);
    JsonAdapter<StorageConfig> storageAdapter = SetAdapter<StorageConfig>(path_storage);
    JsonAdapter<StreamConfig> streamAdapter = SetAdapter<StreamConfig>(path_stream);
    JsonAdapter<CalibrationConfig> calibrationAdapter =
        SetAdapter<CalibrationConfig>(path_calibration);

    ConfigLoader loader(usersAdapter, profileAdapter, deviceAdapter, storageAdapter, streamAdapter,
                        calibrationAdapter);
    auto cfg = loader.get();

    LogObject(cfg, "ConfigData Defaults");
}

TEST(ConfigLoader, ConfigData_UserOverrides) {
    using sst::config::adapters::JsonAdapter;
    using sst::config::app::ConfigLoader;
    using sst::config::domain::CalibrationConfig;
    using sst::config::domain::DeviceConfig;
    using sst::config::domain::ProfileConfig;
    using sst::config::domain::StorageConfig;
    using sst::config::domain::StreamConfig;
    using sst::config::domain::UsersConfig;

    JsonAdapter<UsersConfig> usersAdapter = SetAdapter<UsersConfig>(path_user);
    JsonAdapter<ProfileConfig> profileAdapter = SetAdapter<ProfileConfig>(path_profile);
    JsonAdapter<DeviceConfig> deviceAdapter = SetAdapter<DeviceConfig>(path_device);
    JsonAdapter<StorageConfig> storageAdapter = SetAdapter<StorageConfig>(path_storage);
    JsonAdapter<StreamConfig> streamAdapter = SetAdapter<StreamConfig>(path_stream);
    JsonAdapter<CalibrationConfig> calibrationAdapter =
        SetAdapter<CalibrationConfig>(path_calibration);

    ConfigLoader loader(usersAdapter, profileAdapter, deviceAdapter, storageAdapter, streamAdapter,
                        calibrationAdapter, 1);
    auto cfg = loader.get();

    LogObject(cfg, "ConfigData User 1");

    ASSERT_TRUE(cfg.storage.logs.has_value());
    ASSERT_TRUE(cfg.storage.logs->enabled.has_value());
    EXPECT_FALSE(*cfg.storage.logs->enabled);
}
