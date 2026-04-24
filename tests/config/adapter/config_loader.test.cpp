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

namespace fs = std::filesystem;

template <typename T>
auto LogObject(const T& object, std::string_view label = "Object") -> void {
    spdlog::info("{}:\n{}", label, object);
}

namespace {
constexpr const char* kRootRel = "tests/config/config_files";

auto RootDir() -> fs::path { return fs::path{SST_REPO_ROOT_DIR} / kRootRel; }

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
    using sst::config::CalibrationCameraDeviceData;
    using sst::config::CalibrationCamerasData;
    using sst::config::CalibrationConfig;
    using sst::config::CalibrationData;

    CalibrationConfig calibrationConfig;
    CalibrationCameraDeviceData cameraDeviceData{
        .last_calibration_date = CalibrationChange,
    };
    CalibrationCamerasData camerasData{
        .device = std::vector<CalibrationCameraDeviceData>{cameraDeviceData},
    };
    CalibrationData userData{.cameras = camerasData};
    calibrationConfig.users.push_back({.id = 1, .user_data = userData});
    return calibrationConfig;
}

}  // namespace

class ConfigLoaderTest : public ::testing::Test {
   protected:
    void SetUp() override {
        using sst::config::CalibrationConfig;
        using sst::config::DeviceConfig;
        using sst::config::JsonWriterAdapter;

        const fs::path root = RootDir();

        JsonWriterAdapter<DeviceConfig> device(root / "device.json");
        JsonWriterAdapter<CalibrationConfig> calibration(root / "calibration.json");

        ASSERT_TRUE(device.save(MakeDeviceConfig()).success);
        ASSERT_TRUE(calibration.save(MakeCalibrationConfig()).success);

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

    ASSERT_TRUE(default_cfg_.device.name.has_value());
    EXPECT_EQ(*default_cfg_.device.name, "sst-cam");

    ASSERT_TRUE(default_cfg_.calibration.cameras.has_value());
    ASSERT_TRUE(default_cfg_.calibration.cameras->device.has_value());
    EXPECT_FALSE(default_cfg_.calibration.cameras->device->empty());
}

TEST_F(ConfigLoaderTest, UserOverride) {
    LogObject(user_cfg_, "ConfigData User 1");

    ASSERT_TRUE(user_cfg_.device.timezone.has_value());
    EXPECT_EQ(*user_cfg_.device.timezone, DeviceChange);

    ASSERT_TRUE(user_cfg_.calibration.cameras.has_value());
    ASSERT_TRUE(user_cfg_.calibration.cameras->device.has_value());
    ASSERT_FALSE(user_cfg_.calibration.cameras->device->empty());
    EXPECT_EQ(user_cfg_.calibration.cameras->device->front().last_calibration_date,
              CalibrationChange);
}
