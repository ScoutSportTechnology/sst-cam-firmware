#include "config/adapters/json/json_adapter.hpp"

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "config/domain/calibration_config.hpp"
#include "config/domain/device_config.hpp"
#include "config/domain/profile_config.hpp"
#include "config/domain/storage_config.hpp"
#include "config/domain/stream_config.hpp"
#include "config/domain/users_config.hpp"

namespace fs = std::filesystem;

namespace {

// Walk upwards until we find tests/config/config_files/<file>
fs::path find_test_file(const fs::path& filename) {
    fs::path cur = fs::current_path();

    for (int i = 0; i < 10; ++i) {
        fs::path candidate = cur / "tests" / "config" / "config_files" / filename;
        if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
            return candidate;
        }
        if (!cur.has_parent_path()) break;
        cur = cur.parent_path();
    }

    // fallback: try relative to current path directly
    fs::path direct = fs::current_path() / "tests" / "config" / "config_files" / filename;
    return direct;
}

void set_test_logging() {
    // Keep CI output clean; bump to debug if you want more noise
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
}

template <typename T>
void expect_load_ok(const fs::path& file) {
    using sst::config::adapters::JsonAdapter;

    std::string err;
    T cfg{};

    JsonAdapter<T> adapter(file);
    ASSERT_TRUE(adapter.load(cfg, err)) << "load failed for " << file.string() << " : " << err;
    ASSERT_TRUE(err.empty()) << "error string not empty for " << file.string() << " : " << err;
}

template <typename T>
void expect_save_roundtrip_ok(const fs::path& src_file, const std::string& temp_name) {
    using sst::config::adapters::JsonAdapter;

    std::string err;

    // Load from the golden test file
    T cfg{};
    {
        JsonAdapter<T> adapter(src_file);
        ASSERT_TRUE(adapter.load(cfg, err))
            << "load failed for " << src_file.string() << " : " << err;
        ASSERT_TRUE(err.empty());
    }

    // Save to temp
    fs::path tmp_dir = fs::temp_directory_path() / "sst_cam_tests";
    fs::create_directories(tmp_dir);
    fs::path tmp_file = tmp_dir / temp_name;

    {
        JsonAdapter<T> adapter(tmp_file);
        err.clear();
        ASSERT_TRUE(adapter.save(cfg, err))
            << "save failed for " << tmp_file.string() << " : " << err;
        ASSERT_TRUE(err.empty());
        ASSERT_TRUE(fs::exists(tmp_file));
    }

    // Reload from temp and just ensure it parses (struct equality is optional/not assumed)
    {
        T cfg2{};
        JsonAdapter<T> adapter(tmp_file);
        err.clear();
        ASSERT_TRUE(adapter.load(cfg2, err))
            << "reload failed for " << tmp_file.string() << " : " << err;
        ASSERT_TRUE(err.empty());
    }

    // Cleanup (best-effort)
    std::error_code ec;
    fs::remove(tmp_file, ec);
}

}  // namespace

TEST(JsonAdapter, Users_LoadAndRoundtrip) {
    set_test_logging();

    using sst::config::domain::UsersConfig;

    const fs::path file = find_test_file("user.json");
    ASSERT_TRUE(fs::exists(file)) << "Missing test file: " << file.string();

    expect_load_ok<UsersConfig>(file);
    expect_save_roundtrip_ok<UsersConfig>(file, "users_roundtrip.json");
}

TEST(JsonAdapter, Profile_LoadAndRoundtrip) {
    set_test_logging();

    using sst::config::domain::ProfileConfig;

    const fs::path file = find_test_file("profile.json");
    ASSERT_TRUE(fs::exists(file)) << "Missing test file: " << file.string();

    expect_load_ok<ProfileConfig>(file);
    expect_save_roundtrip_ok<ProfileConfig>(file, "profile_roundtrip.json");
}

TEST(JsonAdapter, Device_LoadAndRoundtrip) {
    set_test_logging();

    using sst::config::domain::DeviceConfig;

    const fs::path file = find_test_file("device.json");
    ASSERT_TRUE(fs::exists(file)) << "Missing test file: " << file.string();

    expect_load_ok<DeviceConfig>(file);
    expect_save_roundtrip_ok<DeviceConfig>(file, "device_roundtrip.json");
}

TEST(JsonAdapter, Calibration_LoadAndRoundtrip) {
    set_test_logging();

    using sst::config::domain::CalibrationConfig;

    const fs::path file = find_test_file("calibration.json");
    ASSERT_TRUE(fs::exists(file)) << "Missing test file: " << file.string();

    expect_load_ok<CalibrationConfig>(file);
    expect_save_roundtrip_ok<CalibrationConfig>(file, "calibration_roundtrip.json");
}

TEST(JsonAdapter, Storage_LoadAndRoundtrip) {
    set_test_logging();

    using sst::config::domain::StorageConfig;

    const fs::path file = find_test_file("storage.json");
    ASSERT_TRUE(fs::exists(file)) << "Missing test file: " << file.string();

    expect_load_ok<StorageConfig>(file);
    expect_save_roundtrip_ok<StorageConfig>(file, "storage_roundtrip.json");
}

TEST(JsonAdapter, Stream_LoadAndRoundtrip) {
    set_test_logging();

    using sst::config::domain::StreamConfig;

    const fs::path file = find_test_file("stream.json");
    ASSERT_TRUE(fs::exists(file)) << "Missing test file: " << file.string();

    expect_load_ok<StreamConfig>(file);
    expect_save_roundtrip_ok<StreamConfig>(file, "stream_roundtrip.json");
}
