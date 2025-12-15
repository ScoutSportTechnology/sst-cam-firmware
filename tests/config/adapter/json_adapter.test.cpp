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

// ---------------- User Config Tests ----------------
// ---------------- Load Test ----------------
TEST(JsonAdapter, User_Load) {
    using sst::config::domain::UsersConfig;

    auto loaded = load_config<sst::config::domain::UsersConfig>(path_user);
    log_object(loaded.data);
    EXPECT_TRUE(loaded.success);
}
// ---------------- Save Test ----------------
TEST(JsonAdapter, User_Save) {
    using sst::config::domain::UsersConfig;

    UsersConfig userConfig;

    userConfig.users.push_back({1, {"Alice"}});
    userConfig.users.push_back({2, {"Bob2"}});

    auto saved = save_config<sst::config::domain::UsersConfig>(path_user, userConfig);
    log_object(saved.data);
    EXPECT_TRUE(saved.success);
}
// ---------------- Data Test ----------------
TEST(JsonAdapter, Users_Data) {}
// ---------------- Reset Test ----------------
TEST(JsonAdapter, User_Reset) {
    using sst::config::domain::UsersConfig;

    auto reset = reset_config<sst::config::domain::UsersConfig>(path_user);
    log_object(reset.data);
    EXPECT_TRUE(reset.success);
}

// ---------------- Profile Config Tests ----------------
// ---------------- Load Test ----------------
TEST(JsonAdapter, Profile_Load) {
    using sst::config::domain::ProfileConfig;

    auto loaded = load_config<sst::config::domain::ProfileConfig>(path_profile);
    log_object(loaded.data);
    EXPECT_TRUE(loaded.success);
}
// ---------------- Save Test ----------------
TEST(JsonAdapter, Profile_Save) {
    using sst::config::domain::ProfileConfig;

    ProfileConfig profileConfig;
    profileConfig.users.push_back({1,
                                   {
                                       .calibration = true,
                                       .device = false,
                                       .stream = true,
                                       .storage = false,
                                   }});

    auto saved = save_config<sst::config::domain::ProfileConfig>(path_profile, profileConfig);
    log_object(saved.data);
    EXPECT_TRUE(saved.success);
}
// ---------------- Data Test ----------------
TEST(JsonAdapter, Profile_Data) {}
// ---------------- Reset Test ----------------
TEST(JsonAdapter, Profile_Reset) {
    using sst::config::domain::ProfileConfig;

    auto reset = reset_config<sst::config::domain::ProfileConfig>(path_profile);
    log_object(reset.data);
    EXPECT_TRUE(reset.success);
}

// ---------------- Device Config Tests ----------------
// ---------------- Load Test ----------------
TEST(JsonAdapter, Device_Load) {
    using sst::config::domain::DeviceConfig;

    auto loaded = load_config<sst::config::domain::DeviceConfig>(path_device);
    log_object(loaded.data);
    EXPECT_TRUE(loaded.success);
}
// ---------------- Save Test ----------------
TEST(JsonAdapter, Device_Save) {
    using sst::config::domain::DeviceConfig;
    using sst::config::domain::DeviceData;

    DeviceData userData{.timezone = "UTC"};

    DeviceConfig deviceConfig;
    deviceConfig.users.push_back({.id = 1, .user_data = userData});

    auto saved = save_config<sst::config::domain::DeviceConfig>(path_device, deviceConfig);
    log_object(saved.data);
    EXPECT_TRUE(saved.success);
}
// ---------------- Data Test ----------------
TEST(JsonAdapter, Device_Data) {}
// ---------------- Reset Test ----------------
TEST(JsonAdapter, Device_Reset) {
    using sst::config::domain::DeviceConfig;

    auto reset = reset_config<sst::config::domain::DeviceConfig>(path_device);
    log_object(reset.data);
    EXPECT_TRUE(reset.success);
}

// ---------------- Calibration Config Tests ----------------
// ---------------- Load Test ----------------
TEST(JsonAdapter, Calibration_Load) {
    using sst::config::domain::CalibrationConfig;

    auto loaded = load_config<sst::config::domain::CalibrationConfig>(path_calibration);
    log_object(loaded.data);
    EXPECT_TRUE(loaded.success);
}
// ---------------- Save Test ----------------
TEST(JsonAdapter, Calibration_Save) {
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

    calibrationConfig.users.push_back({.id = 1, .user_data = userData});

    auto saved =
        save_config<sst::config::domain::CalibrationConfig>(path_calibration, calibrationConfig);
    log_object(saved.data);
    EXPECT_TRUE(saved.success);
}
// ---------------- Data Test ----------------
TEST(JsonAdapter, Calibration_Data) {}
// ---------------- Reset Test ----------------
TEST(JsonAdapter, Calibration_Reset) {
    using sst::config::domain::CalibrationConfig;

    auto reset = reset_config<sst::config::domain::CalibrationConfig>(path_calibration);
    log_object(reset.data);
    EXPECT_TRUE(reset.success);
}

// ---------------- Storage Config Tests ----------------
// ---------------- Load Test ----------------
TEST(JsonAdapter, Storage_Load) {
    using sst::config::domain::StorageConfig;

    auto loaded = load_config<sst::config::domain::StorageConfig>(path_storage);
    log_object(loaded.data);
    EXPECT_TRUE(loaded.success);
}
// ---------------- Save Test ----------------
TEST(JsonAdapter, Storage_Save) {
    using sst::config::domain::StorageConfig;
    using sst::config::domain::StorageData;
    using sst::config::domain::StorageSectionData;

    StorageConfig storageConfig;
    StorageSectionData logsData{.enabled = false};
    StorageData userData{.logs = logsData};

    storageConfig.users.push_back({.id = 1, .user_data = userData});

    auto saved = save_config<sst::config::domain::StorageConfig>(path_storage, storageConfig);
    log_object(saved.data);
    EXPECT_TRUE(saved.success);
}
// ---------------- Data Test ----------------
TEST(JsonAdapter, Storage_Data) {}
// ---------------- Reset Test ----------------
TEST(JsonAdapter, Storage_Reset) {
    using sst::config::domain::StorageConfig;

    auto reset = reset_config<sst::config::domain::StorageConfig>(path_storage);
    log_object(reset.data);
    EXPECT_TRUE(reset.success);
}

// ---------------- Stream Config Tests ----------------
// ---------------- Load Test ----------------
TEST(JsonAdapter, Stream_Load) {
    using sst::config::domain::StreamConfig;

    auto loaded = load_config<sst::config::domain::StreamConfig>(path_stream);
    log_object(loaded.data);
    EXPECT_TRUE(loaded.success);
}
// ---------------- Save Test ----------------
TEST(JsonAdapter, Stream_Save) {
    using sst::config::domain::StreamConfig;
    using sst::config::domain::StreamData;
    using sst::config::domain::StreamPlatformData;
    using sst::config::domain::StreamPlatformSettingsData;

    StreamConfig streamConfig;
    StreamPlatformData youtubeData{.enabled = false};
    StreamData userData{.youtube = youtubeData};

    streamConfig.users.push_back({.id = 1, .user_data = userData});

    auto saved = save_config<sst::config::domain::StreamConfig>(path_stream, streamConfig);
    log_object(saved.data);
    EXPECT_TRUE(saved.success);
}
// ---------------- Data Test ----------------
TEST(JsonAdapter, Stream_Data) {}
// ---------------- Reset Test ----------------
TEST(JsonAdapter, Stream_Reset) {
    using sst::config::domain::StreamConfig;

    auto reset = reset_config<sst::config::domain::StreamConfig>(path_stream);
    log_object(reset.data);
    EXPECT_TRUE(reset.success);
}
