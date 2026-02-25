#include "adapters/config/reader/json/json.hpp"
#include "adapters/config/writer/json/json.hpp"

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>

#include "domain/config/models/calibration.hpp"
#include "domain/config/models/device.hpp"
#include "domain/config/models/profile.hpp"
#include "domain/config/models/storage.hpp"
#include "domain/config/models/stream.hpp"
#include "domain/config/models/users.hpp"

namespace fs = std::filesystem;
using sst::config::ports::ConfigReturn;

namespace test::utils {
using sst::config::adapters::JsonReaderAdapter;
using sst::config::adapters::JsonWriterAdapter;

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

struct UsersCase {
    using Config = sst::config::domain::UsersConfig;
    static constexpr std::string_view label = "UsersConfig";
    static constexpr const char* path = "tests/config/config_files/users.json";
    static auto Make() -> Config {
        Config userConfig;
        userConfig.users.push_back({1, {"Alice"}});
        userConfig.users.push_back({2, {"Bob"}});
        return userConfig;
    }
};

struct ProfileCase {
    using Config = sst::config::domain::ProfileConfig;
    static constexpr std::string_view label = "ProfileConfig";
    static constexpr const char* path = "tests/config/config_files/profile.json";
    static auto Make() -> Config {
        Config profileConfig;
        profileConfig.users.push_back({1,
                                       {
                                           .calibration = false,
                                           .device = false,
                                           .stream = false,
                                           .storage = true,
                                       }});
        return profileConfig;
    }
};

struct DeviceCase {
    using Config = sst::config::domain::DeviceConfig;
    using Data = sst::config::domain::DeviceData;
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
    using Config = sst::config::domain::CalibrationConfig;
    static constexpr std::string_view label = "CalibrationConfig";
    static constexpr const char* path = "tests/config/config_files/calibration.json";
    static auto Make() -> Config {
        using sst::config::domain::CalibrationCameraData;
        using sst::config::domain::CalibrationData;
        using sst::config::domain::CalibrationDevicesData;

        Config calibrationConfig;
        CalibrationCameraData cameraData{
            .last_calibration_date = "2024-05-05",
        };
        CalibrationDevicesData devicesData{.camera = std::vector<CalibrationCameraData>{cameraData},
                                           .microphone = {}};

        CalibrationData userData{.devices = devicesData};

        calibrationConfig.users.push_back({.id = 2, .user_data = userData});
        return calibrationConfig;
    }
};

struct StorageCase {
    using Config = sst::config::domain::StorageConfig;
    static constexpr std::string_view label = "StorageConfig";
    static constexpr const char* path = "tests/config/config_files/storage.json";
    static auto Make() -> Config {
        using sst::config::domain::StorageData;
        using sst::config::domain::StorageSectionData;

        Config storageConfig;
        StorageSectionData logsData{.enabled = false};
        StorageData userData{.logs = logsData};

        storageConfig.users.push_back({.id = 1, .user_data = userData});
        return storageConfig;
    }
};

struct StreamCase {
    using Config = sst::config::domain::StreamConfig;
    static constexpr std::string_view label = "StreamConfig";
    static constexpr const char* path = "tests/config/config_files/stream.json";
    static auto Make() -> Config {
        using sst::config::domain::StreamData;
        using sst::config::domain::StreamPlatformData;

        Config streamConfig;
        StreamPlatformData youtubeData{.enabled = false};
        StreamData userData{.youtube = youtubeData};

        streamConfig.users.push_back({.id = 2, .user_data = userData});
        return streamConfig;
    }
};

using ConfigCases =
    ::testing::Types<UsersCase, ProfileCase, DeviceCase, CalibrationCase, StorageCase, StreamCase>;

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
