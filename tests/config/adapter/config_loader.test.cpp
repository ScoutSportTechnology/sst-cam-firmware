#include "config/app/config_loader.hpp"

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

#include "config/adapters/json/json_adapter.hpp"
#include "config/domain/calibration_config.hpp"
#include "config/domain/device_config.hpp"
#include "config/domain/formatter/config_data.fmt.hpp"  // IWYU pragma: keep
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

namespace {

auto MakeUsersConfig() -> sst::config::domain::UsersConfig {
    using sst::config::domain::UsersConfig;

    UsersConfig userConfig;
    userConfig.users.push_back({1, {"Alice"}});
    return userConfig;
}

auto MakeProfileConfig() -> sst::config::domain::ProfileConfig {
    using sst::config::domain::ProfileConfig;

    ProfileConfig profileConfig;
    profileConfig.users.push_back({1,
                                   {
                                       .calibration = true,
                                       .device = true,
                                       .stream = true,
                                       .storage = true,
                                   }});
    return profileConfig;
}

const char* DeviceChange = "EST";
auto MakeDeviceConfig() -> sst::config::domain::DeviceConfig {
    using sst::config::domain::DeviceConfig;
    using sst::config::domain::DeviceData;

    DeviceData userData{.timezone = DeviceChange};

    DeviceConfig deviceConfig;
    deviceConfig.users.push_back({.id = 1, .user_data = userData});
    return deviceConfig;
}

const char* CalibrationChange = "2024-06-01";
auto MakeCalibrationConfig() -> sst::config::domain::CalibrationConfig {
    using sst::config::domain::CalibrationCameraData;
    using sst::config::domain::CalibrationConfig;
    using sst::config::domain::CalibrationData;
    using sst::config::domain::CalibrationDevicesData;

    CalibrationConfig calibrationConfig;
    CalibrationCameraData cameraData{
        .last_calibration_date = CalibrationChange,
    };
    CalibrationDevicesData devicesData{.camera = std::vector<CalibrationCameraData>{cameraData},
                                       .microphone = {}};

    CalibrationData userData{.devices = devicesData};

    calibrationConfig.users.push_back({.id = 1, .user_data = userData});
    return calibrationConfig;
}

const bool StorageChange = false;
auto MakeStorageConfig() -> sst::config::domain::StorageConfig {
    using sst::config::domain::StorageConfig;
    using sst::config::domain::StorageData;
    using sst::config::domain::StorageSectionData;

    StorageConfig storageConfig;
    StorageSectionData logsData{.enabled = StorageChange};
    StorageData userData{.logs = logsData};

    storageConfig.users.push_back({.id = 1, .user_data = userData});
    return storageConfig;
}

const bool StreamChange = false;
auto MakeStreamConfig() -> sst::config::domain::StreamConfig {
    using sst::config::domain::StreamConfig;
    using sst::config::domain::StreamData;
    using sst::config::domain::StreamPlatformData;

    StreamConfig streamConfig;
    StreamPlatformData youtubeData{.enabled = StreamChange};
    StreamData userData{.youtube = youtubeData};

    streamConfig.users.push_back({.id = 1, .user_data = userData});
    return streamConfig;
}

}  // namespace

class ConfigLoader : public ::testing::Test {
   protected:
    void SetUp() override {
        using sst::config::adapters::JsonAdapter;
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

        ASSERT_TRUE(usersAdapter.save(MakeUsersConfig()).success);
        ASSERT_TRUE(profileAdapter.save(MakeProfileConfig()).success);
        ASSERT_TRUE(deviceAdapter.save(MakeDeviceConfig()).success);
        ASSERT_TRUE(storageAdapter.save(MakeStorageConfig()).success);
        ASSERT_TRUE(streamAdapter.save(MakeStreamConfig()).success);
        ASSERT_TRUE(calibrationAdapter.save(MakeCalibrationConfig()).success);

        sst::config::app::ConfigLoader default_loader(usersAdapter, profileAdapter, deviceAdapter,
                                                      storageAdapter, streamAdapter,
                                                      calibrationAdapter);
        default_cfg_ = default_loader.get();

        sst::config::app::ConfigLoader user_loader(usersAdapter, profileAdapter, deviceAdapter,
                                                   storageAdapter, streamAdapter,
                                                   calibrationAdapter, 1);
        user_cfg_ = user_loader.get();
    }

    sst::config::domain::ConfigData default_cfg_{};
    sst::config::domain::ConfigData user_cfg_{};
};

TEST_F(ConfigLoader, Defaults) {
    LogObject(default_cfg_, "ConfigData Defaults");

    ASSERT_TRUE(default_cfg_.storage.logs.has_value());
    ASSERT_TRUE(default_cfg_.storage.logs->enabled.has_value());
    EXPECT_TRUE(*default_cfg_.storage.logs->enabled);

    ASSERT_TRUE(default_cfg_.stream.youtube.has_value());
    ASSERT_TRUE(default_cfg_.stream.youtube->enabled.has_value());
    EXPECT_TRUE(*default_cfg_.stream.youtube->enabled);
}

TEST_F(ConfigLoader, UserOverride) {
    LogObject(user_cfg_, "ConfigData User 1");

    // Storage
    ASSERT_TRUE(user_cfg_.storage.logs.has_value());
    ASSERT_TRUE(user_cfg_.storage.logs->enabled.has_value());
    EXPECT_FALSE(*user_cfg_.storage.logs->enabled);

    // Stream
    ASSERT_TRUE(user_cfg_.stream.youtube.has_value());
    ASSERT_TRUE(user_cfg_.stream.youtube->enabled.has_value());
    EXPECT_FALSE(*user_cfg_.stream.youtube->enabled);

    // Device
    ASSERT_TRUE(user_cfg_.device.timezone.has_value());
    EXPECT_EQ(*user_cfg_.device.timezone, DeviceChange);

    // Calibration
    ASSERT_TRUE(user_cfg_.calibration.devices.has_value());
    ASSERT_TRUE(user_cfg_.calibration.devices->camera.has_value());
    ASSERT_FALSE(user_cfg_.calibration.devices->camera->empty());
    EXPECT_EQ(user_cfg_.calibration.devices->camera->front().last_calibration_date,
              CalibrationChange);
}