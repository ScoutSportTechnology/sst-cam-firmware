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

TEST(JsonAdapter, Users) {
    using sst::config::domain::UsersConfig;

    std::string path = "tests/config/config_files/user.json";

    sst::config::adapters::JsonAdapter<UsersConfig> adapter(fs::path{path});
}

TEST(JsonAdapter, Profile) {
    using sst::config::domain::ProfileConfig;

    std::string path = "tests/config/config_files/profile.json";

    sst::config::adapters::JsonAdapter<ProfileConfig> adapter(fs::path{path});
}

TEST(JsonAdapter, Device) {
    using sst::config::domain::DeviceConfig;

    std::string path = "tests/config/config_files/device.json";
    sst::config::adapters::JsonAdapter<DeviceConfig> adapter(fs::path{path});
}

TEST(JsonAdapter, Calibration) {
    using sst::config::domain::CalibrationConfig;

    std::string path = "tests/config/config_files/calibration.json";
    sst::config::adapters::JsonAdapter<CalibrationConfig> adapter(fs::path{path});
}

TEST(JsonAdapter, Storage) {
    using sst::config::domain::StorageConfig;

    std::string path = "tests/config/config_files/storage.json";
    sst::config::adapters::JsonAdapter<StorageConfig> adapter(fs::path{path});
}

TEST(JsonAdapter, Stream) {
    using sst::config::domain::StreamConfig;

    std::string path = "tests/config/config_files/stream.json";
    sst::config::adapters::JsonAdapter<StreamConfig> adapter(fs::path{path});
}
