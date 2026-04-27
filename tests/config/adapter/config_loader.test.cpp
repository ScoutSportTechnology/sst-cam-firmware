#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <filesystem>

#include "app/config/services/config_loader/config-loader.hpp"
#include "domain/config/models/config-data.hpp"
#include "domain/config/models/formatter/_fmt.hpp"  // IWYU pragma: keep

namespace fs = std::filesystem;

namespace {
constexpr const char* kConfigDir = "tests/config/config_files";
}  // namespace

TEST(ConfigLoaderTest, LoadAndLog) {
    const fs::path root = fs::path{SST_REPO_ROOT_DIR} / kConfigDir;
    sst::config::app::ConfigLoader loader(root.string(), "json");
    sst::config::ConfigData cfg = loader.get();
    spdlog::info("ConfigData:\n{}", cfg);
}
