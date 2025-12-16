#include "config/adapters/json/json_adapter.hpp"

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>

#include "config/domain/calibration_config.hpp"
#include "config/domain/device_config.hpp"
#include "config/domain/profile_config.hpp"
#include "config/domain/storage_config.hpp"
#include "config/domain/stream_config.hpp"
#include "config/domain/users_config.hpp"

namespace fs = std::filesystem;
using sst::config::ports::ConfigReturn;

template <typename T>
auto load_config(const std::string& path_str) -> ConfigReturn<T> {
    fs::path path = fs::path{SST_REPO_ROOT_DIR} / path_str;
    sst::config::adapters::JsonAdapter<T> adapter(path);
    return adapter.load();
}

template <typename T>
auto save_config(const std::string& path_str, const T& config) -> ConfigReturn<T> {
    fs::path path = fs::path{SST_REPO_ROOT_DIR} / path_str;
    sst::config::adapters::JsonAdapter<T> adapter(path);

    return adapter.save(config);
}

template <typename T>
auto reset_config(const std::string& path_str) -> ConfigReturn<T> {
    fs::path path = fs::path{SST_REPO_ROOT_DIR} / path_str;
    sst::config::adapters::JsonAdapter<T> adapter(path);

    return adapter.reset();
}

template <typename T>
auto log_object(const T& obj, std::string_view label = "Object") -> void {
    spdlog::info("{}:\n{}", label, nlohmann::json(obj).dump(4));
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
    userConfig.users.push_back({2, {"Bob"}});
    return userConfig;
}

auto MakeProfileConfig() -> sst::config::domain::ProfileConfig {
    using sst::config::domain::ProfileConfig;

    ProfileConfig profileConfig;
    profileConfig.users.push_back({1,
                                   {
                                       .calibration = false,
                                       .device = false,
                                       .stream = false,
                                       .storage = true,
                                   }});
    return profileConfig;
}

auto MakeDeviceConfig() -> sst::config::domain::DeviceConfig {
    using sst::config::domain::DeviceConfig;
    using sst::config::domain::DeviceData;

    DeviceData userData{.timezone = "UTC"};

    DeviceConfig deviceConfig;
    deviceConfig.users.push_back({.id = 2, .user_data = userData});
    return deviceConfig;
}

auto MakeCalibrationConfig() -> sst::config::domain::CalibrationConfig {
    using sst::config::domain::CalibrationCameraData;
    using sst::config::domain::CalibrationConfig;
    using sst::config::domain::CalibrationData;
    using sst::config::domain::CalibrationDevicesData;

    CalibrationConfig calibrationConfig;
    CalibrationCameraData cameraData{
        .last_calibration_date = "2024-05-05",
    };
    CalibrationDevicesData devicesData{.camera = std::vector<CalibrationCameraData>{cameraData},
                                       .microphone = {}};

    CalibrationData userData{.devices = devicesData};

    calibrationConfig.users.push_back({.id = 2, .user_data = userData});
    return calibrationConfig;
}

auto MakeStorageConfig() -> sst::config::domain::StorageConfig {
    using sst::config::domain::StorageConfig;
    using sst::config::domain::StorageData;
    using sst::config::domain::StorageSectionData;

    StorageConfig storageConfig;
    StorageSectionData logsData{.enabled = false};
    StorageData userData{.logs = logsData};

    storageConfig.users.push_back({.id = 1, .user_data = userData});
    return storageConfig;
}

auto MakeStreamConfig() -> sst::config::domain::StreamConfig {
    using sst::config::domain::StreamConfig;
    using sst::config::domain::StreamData;
    using sst::config::domain::StreamPlatformData;

    StreamConfig streamConfig;
    StreamPlatformData youtubeData{.enabled = false};
    StreamData userData{.youtube = youtubeData};

    streamConfig.users.push_back({.id = 2, .user_data = userData});
    return streamConfig;
}

}  // namespace

TEST(JsonAdapter, Load_Users) {
    auto loaded = load_config<sst::config::domain::UsersConfig>(path_user);
    log_object(loaded.data, "UsersConfig");
    EXPECT_TRUE(loaded.success);
}

TEST(JsonAdapter, Load_Profile) {
    auto loaded = load_config<sst::config::domain::ProfileConfig>(path_profile);
    log_object(loaded.data, "ProfileConfig");
    EXPECT_TRUE(loaded.success);
}

TEST(JsonAdapter, Load_Device) {
    auto loaded = load_config<sst::config::domain::DeviceConfig>(path_device);
    log_object(loaded.data, "DeviceConfig");
    EXPECT_TRUE(loaded.success);
}

TEST(JsonAdapter, Load_Calibration) {
    auto loaded = load_config<sst::config::domain::CalibrationConfig>(path_calibration);
    log_object(loaded.data, "CalibrationConfig");
    EXPECT_TRUE(loaded.success);
}

TEST(JsonAdapter, Load_Storage) {
    auto loaded = load_config<sst::config::domain::StorageConfig>(path_storage);
    log_object(loaded.data, "StorageConfig");
    EXPECT_TRUE(loaded.success);
}

TEST(JsonAdapter, Load_Stream) {
    auto loaded = load_config<sst::config::domain::StreamConfig>(path_stream);
    log_object(loaded.data, "StreamConfig");
    EXPECT_TRUE(loaded.success);
}

TEST(JsonAdapter, Save_Users) {
    auto saved = save_config<sst::config::domain::UsersConfig>(path_user, MakeUsersConfig());
    log_object(saved.data, "UsersConfig");
    EXPECT_TRUE(saved.success);
}

TEST(JsonAdapter, Save_Profile) {
    auto saved = save_config<sst::config::domain::ProfileConfig>(path_profile, MakeProfileConfig());
    log_object(saved.data, "ProfileConfig");
    EXPECT_TRUE(saved.success);
}

TEST(JsonAdapter, Save_Device) {
    auto saved = save_config<sst::config::domain::DeviceConfig>(path_device, MakeDeviceConfig());
    log_object(saved.data, "DeviceConfig");
    EXPECT_TRUE(saved.success);
}

TEST(JsonAdapter, Save_Calibration) {
    auto saved = save_config<sst::config::domain::CalibrationConfig>(path_calibration,
                                                                     MakeCalibrationConfig());
    log_object(saved.data, "CalibrationConfig");
    EXPECT_TRUE(saved.success);
}

TEST(JsonAdapter, Save_Storage) {
    auto saved = save_config<sst::config::domain::StorageConfig>(path_storage, MakeStorageConfig());
    log_object(saved.data, "StorageConfig");
    EXPECT_TRUE(saved.success);
}

TEST(JsonAdapter, Save_Stream) {
    auto saved = save_config<sst::config::domain::StreamConfig>(path_stream, MakeStreamConfig());
    log_object(saved.data, "StreamConfig");
    EXPECT_TRUE(saved.success);
}

TEST(JsonAdapter, Reset_Users) {
    auto reset = reset_config<sst::config::domain::UsersConfig>(path_user);
    log_object(reset.data, "UsersConfig");
    EXPECT_TRUE(reset.success);
}

TEST(JsonAdapter, Reset_Profile) {
    auto reset = reset_config<sst::config::domain::ProfileConfig>(path_profile);
    log_object(reset.data, "ProfileConfig");
    EXPECT_TRUE(reset.success);
}

TEST(JsonAdapter, Reset_Device) {
    auto reset = reset_config<sst::config::domain::DeviceConfig>(path_device);
    log_object(reset.data, "DeviceConfig");
    EXPECT_TRUE(reset.success);
}

TEST(JsonAdapter, Reset_Calibration) {
    auto reset = reset_config<sst::config::domain::CalibrationConfig>(path_calibration);
    log_object(reset.data, "CalibrationConfig");
    EXPECT_TRUE(reset.success);
}

TEST(JsonAdapter, Reset_Storage) {
    auto reset = reset_config<sst::config::domain::StorageConfig>(path_storage);
    log_object(reset.data, "StorageConfig");
    EXPECT_TRUE(reset.success);
}

TEST(JsonAdapter, Reset_Stream) {
    auto reset = reset_config<sst::config::domain::StreamConfig>(path_stream);
    log_object(reset.data, "StreamConfig");
    EXPECT_TRUE(reset.success);
}

TEST(JsonAdapter, Load_AllConfigs) {
    {
        auto loaded = load_config<sst::config::domain::UsersConfig>(path_user);
        log_object(loaded.data, "UsersConfig");
        EXPECT_TRUE(loaded.success);
    }
    {
        auto loaded = load_config<sst::config::domain::ProfileConfig>(path_profile);
        log_object(loaded.data, "ProfileConfig");
        EXPECT_TRUE(loaded.success);
    }
    {
        auto loaded = load_config<sst::config::domain::DeviceConfig>(path_device);
        log_object(loaded.data, "DeviceConfig");
        EXPECT_TRUE(loaded.success);
    }
    {
        auto loaded = load_config<sst::config::domain::CalibrationConfig>(path_calibration);
        log_object(loaded.data, "CalibrationConfig");
        EXPECT_TRUE(loaded.success);
    }
    {
        auto loaded = load_config<sst::config::domain::StorageConfig>(path_storage);
        log_object(loaded.data, "StorageConfig");
        EXPECT_TRUE(loaded.success);
    }
    {
        auto loaded = load_config<sst::config::domain::StreamConfig>(path_stream);
        log_object(loaded.data, "StreamConfig");
        EXPECT_TRUE(loaded.success);
    }
}

TEST(JsonAdapter, Save_AllConfigs) {
    {
        auto saved = save_config<sst::config::domain::UsersConfig>(path_user, MakeUsersConfig());
        log_object(saved.data, "UsersConfig");
        EXPECT_TRUE(saved.success);
    }
    {
        auto saved =
            save_config<sst::config::domain::ProfileConfig>(path_profile, MakeProfileConfig());
        log_object(saved.data, "ProfileConfig");
        EXPECT_TRUE(saved.success);
    }
    {
        auto saved =
            save_config<sst::config::domain::DeviceConfig>(path_device, MakeDeviceConfig());
        log_object(saved.data, "DeviceConfig");
        EXPECT_TRUE(saved.success);
    }
    {
        auto saved = save_config<sst::config::domain::CalibrationConfig>(path_calibration,
                                                                         MakeCalibrationConfig());
        log_object(saved.data, "CalibrationConfig");
        EXPECT_TRUE(saved.success);
    }
    {
        auto saved =
            save_config<sst::config::domain::StorageConfig>(path_storage, MakeStorageConfig());
        log_object(saved.data, "StorageConfig");
        EXPECT_TRUE(saved.success);
    }
    {
        auto saved =
            save_config<sst::config::domain::StreamConfig>(path_stream, MakeStreamConfig());
        log_object(saved.data, "StreamConfig");
        EXPECT_TRUE(saved.success);
    }
}

TEST(JsonAdapter, Reset_AllConfigs) {
    {
        auto reset = reset_config<sst::config::domain::UsersConfig>(path_user);
        log_object(reset.data, "UsersConfig");
        EXPECT_TRUE(reset.success);
    }
    {
        auto reset = reset_config<sst::config::domain::ProfileConfig>(path_profile);
        log_object(reset.data, "ProfileConfig");
        EXPECT_TRUE(reset.success);
    }
    {
        auto reset = reset_config<sst::config::domain::DeviceConfig>(path_device);
        log_object(reset.data, "DeviceConfig");
        EXPECT_TRUE(reset.success);
    }
    {
        auto reset = reset_config<sst::config::domain::CalibrationConfig>(path_calibration);
        log_object(reset.data, "CalibrationConfig");
        EXPECT_TRUE(reset.success);
    }
    {
        auto reset = reset_config<sst::config::domain::StorageConfig>(path_storage);
        log_object(reset.data, "StorageConfig");
        EXPECT_TRUE(reset.success);
    }
    {
        auto reset = reset_config<sst::config::domain::StreamConfig>(path_stream);
        log_object(reset.data, "StreamConfig");
        EXPECT_TRUE(reset.success);
    }
}
