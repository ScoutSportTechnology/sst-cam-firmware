#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>

#include "adapters/config/reader/json/json.hpp"
#include "adapters/config/writer/json/json.hpp"
#include "domain/config/models/calibration.hpp"
#include "domain/config/models/device.hpp"

namespace fs = std::filesystem;
using sst::config::ConfigReturn;

namespace test::utils {
using sst::config::JsonReaderAdapter;
using sst::config::JsonWriterAdapter;

template <typename T>
auto load_config(const std::string& path_str) -> ConfigReturn<T> {
    fs::path path = fs::path{SST_REPO_ROOT_DIR} / path_str;
    JsonReaderAdapter<T> adapter(path);
    return adapter.load();
}

template <typename T>
auto save_config(const std::string& path_str, const T& config) -> ConfigReturn<T> {
    fs::path path = fs::path{SST_REPO_ROOT_DIR} / path_str;
    JsonWriterAdapter<T> adapter(path);

    return adapter.save(config);
}

template <typename T>
auto reset_config(const std::string& path_str) -> ConfigReturn<T> {
    fs::path path = fs::path{SST_REPO_ROOT_DIR} / path_str;
    JsonWriterAdapter<T> adapter(path);

    return adapter.reset();
}

template <typename T>
auto log_object(const T& obj, std::string_view label = "Object") -> void {
    spdlog::info("{}:\n{}", label, nlohmann::json(obj).dump(4));
}
}  // namespace test::utils

struct DeviceCase {
    using Config = sst::config::DeviceConfig;
    using Data = sst::config::DeviceData;
    static constexpr std::string_view label = "DeviceConfig";
    static constexpr const char* path = "tests/config/config_files/device.json";
    static auto Make() -> Config {
        Data userData{.timezone = "UTC"};
        Config deviceConfig;
        deviceConfig.users.push_back({.id = 2, .user_data = userData});
        return deviceConfig;
    }
};

struct CalibrationCase {
    using Config = sst::config::CalibrationConfig;
    static constexpr std::string_view label = "CalibrationConfig";
    static constexpr const char* path = "tests/config/config_files/calibration.json";
    static auto Make() -> Config {
        using sst::config::CalibrationCameraDeviceData;
        using sst::config::CalibrationCamerasData;
        using sst::config::CalibrationData;

        Config calibrationConfig;
        CalibrationCameraDeviceData cameraDeviceData{
            .id = 0,
            .last_calibration_date = "2024-05-05",
        };
        CalibrationCamerasData camerasData{
            .device = std::vector<CalibrationCameraDeviceData>{cameraDeviceData},
        };
        CalibrationData userData{.cameras = camerasData};
        calibrationConfig.users.push_back({.id = 2, .user_data = userData});
        return calibrationConfig;
    }
};

using ConfigCases = ::testing::Types<DeviceCase, CalibrationCase>;

template <typename Case>
class JsonAdapter : public ::testing::Test {};

struct CaseNameGenerator {
    template <typename T>
    static auto GetName(int /*unused*/) -> std::string {
        return std::string(T::label);
    }
};

TYPED_TEST_SUITE(JsonAdapter, ConfigCases, CaseNameGenerator);

TYPED_TEST(JsonAdapter, Load) {
    auto loaded = test::utils::load_config<typename TypeParam::Config>(TypeParam::path);
    test::utils::log_object(loaded.data, TypeParam::label);
    EXPECT_TRUE(loaded.success);
}

TYPED_TEST_SUITE(JsonAdapter, ConfigCases, CaseNameGenerator);

TYPED_TEST(JsonAdapter, Save) {
    auto saved =
        test::utils::save_config<typename TypeParam::Config>(TypeParam::path, TypeParam::Make());
    test::utils::log_object(saved.data, TypeParam::label);
    EXPECT_TRUE(saved.success);
}

TYPED_TEST_SUITE(JsonAdapter, ConfigCases, CaseNameGenerator);

TYPED_TEST(JsonAdapter, Reset) {
    auto reset = test::utils::reset_config<typename TypeParam::Config>(TypeParam::path);
    test::utils::log_object(reset.data, TypeParam::label);
    EXPECT_TRUE(reset.success);
}
