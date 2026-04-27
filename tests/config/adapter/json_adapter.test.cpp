#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>

#include "adapters/config/json/json.hpp"
#include "domain/config/models/calibration.hpp"
#include "domain/config/models/device.hpp"
#include "domain/config/models/formatter/_fmt.hpp"  // IWYU pragma: keep
#include "domain/config/models/storage.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kConfigDir = "tests/config/config_files";

template <typename T>
auto LoadAndLog(const std::string& filename, std::string_view label) -> bool {
    fs::path path = fs::path{SST_REPO_ROOT_DIR} / kConfigDir / filename;
    sst::config::JsonReaderAdapter<T> adapter(path);
    auto result = adapter.load();
    if (!result.success) {
        return false;
    }
    spdlog::info("{}:\n{}", label, result.data);
    return true;
}

}  // namespace

TEST(JsonAdapter, LoadDevice) {
    EXPECT_TRUE(LoadAndLog<sst::config::DeviceData>("device.json", "DeviceData"));
}

TEST(JsonAdapter, LoadCalibration) {
    EXPECT_TRUE(LoadAndLog<sst::config::CalibrationData>("calibration.json", "CalibrationData"));
}

TEST(JsonAdapter, LoadStorage) {
    EXPECT_TRUE(LoadAndLog<sst::config::StorageData>("storage.json", "StorageData"));
}
