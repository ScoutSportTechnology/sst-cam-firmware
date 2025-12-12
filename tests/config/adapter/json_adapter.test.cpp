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

template <typename T>
struct Response {
    bool success{false};
    T data{};
};

template <typename T>
auto load_config(const std::string& path_str) -> Response<T> {
    fs::path path = fs::path{SST_REPO_ROOT_DIR} / path_str;
    sst::config::adapters::JsonAdapter<T> adapter(path);

    Response<T> response;
    response.success = adapter.load(response.data);
    return response;
}

template <typename T>
auto save_config(const std::string& path_str, const T& config) -> Response<T> {
    fs::path path = fs::path{SST_REPO_ROOT_DIR} / path_str;
    sst::config::adapters::JsonAdapter<T> adapter(path);

    Response<T> response;
    response.success = adapter.save(config);
    response.data = config;
    return response;
}

template <typename T>
auto log_object(const T& obj, std::string_view label = "Object") -> void {
    spdlog::info("{}:\n{}", label, nlohmann::json(obj).dump(4));
}

std::string path_user = "tests/config/config_files/users.json";
std::string path_profile = "tests/config/config_files/profile.json";
std::string path_device = "tests/config/config_files/device.json";
std::string path_calibration = "tests/config/config_files/calibration.json";
std::string path_storage = "tests/config/config_files/storage.json";
std::string path_stream = "tests/config/config_files/stream.json";

TEST(JsonAdapter_User, Load) {
    using sst::config::domain::UsersConfig;

    auto loaded = load_config<sst::config::domain::UsersConfig>(path_user);
    log_object(loaded.data);
    EXPECT_TRUE(loaded.success);
}

TEST(JsonAdapter_User, Save) {
    using sst::config::domain::UsersConfig;

    UsersConfig userConfig;

    userConfig.users.push_back({1, {"Alice"}});
    userConfig.users.push_back({2, {"Bob"}});

    auto saved = save_config<sst::config::domain::UsersConfig>(path_user, userConfig);
    log_object(saved.data);
    EXPECT_TRUE(saved.success);
}

TEST(JsonAdapter_Profile, Load) {
    using sst::config::domain::ProfileConfig;

    auto loaded = load_config<sst::config::domain::ProfileConfig>(path_profile);
    log_object(loaded.data);
    EXPECT_TRUE(loaded.success);
}

TEST(JsonAdapter_Device, Load) {
    using sst::config::domain::DeviceConfig;

    auto loaded = load_config<sst::config::domain::DeviceConfig>(path_device);
    log_object(loaded.data);
    EXPECT_TRUE(loaded.success);
}

TEST(JsonAdapter_Calibration, Load) {
    using sst::config::domain::CalibrationConfig;

    auto loaded = load_config<sst::config::domain::CalibrationConfig>(path_calibration);
    log_object(loaded.data);
    EXPECT_TRUE(loaded.success);
}

TEST(JsonAdapter_Storage, Load) {
    using sst::config::domain::StorageConfig;

    auto loaded = load_config<sst::config::domain::StorageConfig>(path_storage);
    log_object(loaded.data);
    EXPECT_TRUE(loaded.success);
}

TEST(JsonAdapter_Stream, Load) {
    using sst::config::domain::StreamConfig;

    auto loaded = load_config<sst::config::domain::StreamConfig>(path_stream);
    log_object(loaded.data);
    EXPECT_TRUE(loaded.success);
}
