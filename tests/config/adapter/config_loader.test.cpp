#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>

#include "adapters/config/reader/json/json.hpp"
#include "adapters/config/writer/json/json.hpp"
#include "app/config/services/config_loader/config-loader.hpp"
#include "domain/config/models/calibration.hpp"
#include "domain/config/models/device.hpp"
#include "domain/config/models/formatter/config-data-fmt.hpp"  // IWYU pragma: keep
#include "domain/config/models/profile.hpp"
#include "domain/config/models/storage.hpp"
#include "domain/config/models/stream.hpp"
#include "domain/config/models/users.hpp"

namespace fs = std::filesystem;

template <typename T>
auto LogObject(const T& object, std::string_view label = "Object") -> void {
    spdlog::info("{}:\n{}", label, object);
}

namespace {
constexpr const char* kRootRel = "tests/config/config_files";

auto RootDir() -> fs::path { return fs::path{SST_REPO_ROOT_DIR} / kRootRel; }

auto MakeUsersConfig() -> sst::config::UsersConfig {
    using sst::config::UsersConfig;

    UsersConfig userConfig;
    userConfig.users.push_back({.id = 1, .user_data = {"Alice"}});
    return userConfig;
}

auto MakeProfileConfig() -> sst::config::ProfileConfig {
    using sst::config::ProfileConfig;

    ProfileConfig profileConfig;
    profileConfig.users.push_back({.id = 1,
                                   .user_data = {
                                       .calibration = true,
                                       .device = true,
                                       .stream = true,
                                       .storage = true,
                                   }});
    return profileConfig;
}

const char* DeviceChange = "EST";
auto MakeDeviceConfig() -> sst::config::DeviceConfig {
    using sst::config::DeviceConfig;
    using sst::config::DeviceData;

    DeviceData userData{.timezone = DeviceChange};

    DeviceConfig deviceConfig;
    deviceConfig.users.push_back({.id = 1, .user_data = userData});
    return deviceConfig;
}

const char* CalibrationChange = "2024-06-01";
auto MakeCalibrationConfig() -> sst::config::CalibrationConfig {
    using sst::config::CalibrationCameraData;
    using sst::config::CalibrationConfig;
    using sst::config::CalibrationData;
    using sst::config::CalibrationDevicesData;

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
auto MakeStorageConfig() -> sst::config::StorageConfig {
    using sst::config::StorageConfig;
    using sst::config::StorageData;
    using sst::config::StorageSectionData;

    StorageConfig storageConfig;
    StorageSectionData logsData{.enabled = StorageChange};
    StorageData userData{.logs = logsData};

    storageConfig.users.push_back({.id = 1, .user_data = userData});
    return storageConfig;
}

const bool StreamChange = false;
auto MakeStreamConfig() -> sst::config::StreamConfig {
    using sst::config::StreamConfig;
    using sst::config::StreamData;
    using sst::config::StreamPlatformData;

    StreamConfig streamConfig;
    StreamPlatformData youtubeData{.enabled = StreamChange};
    StreamData userData{.youtube = youtubeData};

    streamConfig.users.push_back({.id = 1, .user_data = userData});
    return streamConfig;
}

}  // namespace

class ConfigLoaderTest : public ::testing::Test {
   protected:
    void SetUp() override {
        using sst::config::CalibrationConfig;
        using sst::config::DeviceConfig;
        using sst::config::JsonReaderAdapter;
        using sst::config::JsonWriterAdapter;
        using sst::config::ProfileConfig;
        using sst::config::StorageConfig;
        using sst::config::StreamConfig;
        using sst::config::UsersConfig;

        const fs::path root = RootDir();

        // Seed config files
        JsonWriterAdapter<UsersConfig> users(root / "users.json");
        JsonWriterAdapter<ProfileConfig> profile(root / "profile.json");
        JsonWriterAdapter<DeviceConfig> device(root / "device.json");
        JsonWriterAdapter<StorageConfig> storage(root / "storage.json");
        JsonWriterAdapter<StreamConfig> stream(root / "stream.json");
        JsonWriterAdapter<CalibrationConfig> calibration(root / "calibration.json");

        ASSERT_TRUE(users.save(MakeUsersConfig()).success);
        ASSERT_TRUE(profile.save(MakeProfileConfig()).success);
        ASSERT_TRUE(device.save(MakeDeviceConfig()).success);
        ASSERT_TRUE(storage.save(MakeStorageConfig()).success);
        ASSERT_TRUE(stream.save(MakeStreamConfig()).success);
        ASSERT_TRUE(calibration.save(MakeCalibrationConfig()).success);

        // Load using new root-path constructor
        sst::config::app::ConfigLoader default_loader(root.string(), "json");
        default_cfg_ = default_loader.get();

        sst::config::app::ConfigLoader user_loader(root.string(), "json", 1);
        user_cfg_ = user_loader.get();
    }

    sst::config::ConfigData default_cfg_{};
    sst::config::ConfigData user_cfg_{};
};

TEST_F(ConfigLoaderTest, Defaults) {
    LogObject(default_cfg_, "ConfigData Defaults");

    ASSERT_TRUE(default_cfg_.storage.logs.has_value());
    ASSERT_TRUE(default_cfg_.storage.logs->enabled.has_value());
    EXPECT_TRUE(*default_cfg_.storage.logs->enabled);

    ASSERT_TRUE(default_cfg_.stream.youtube.has_value());
    ASSERT_TRUE(default_cfg_.stream.youtube->enabled.has_value());
    EXPECT_TRUE(*default_cfg_.stream.youtube->enabled);
}

TEST_F(ConfigLoaderTest, UserOverride) {
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
