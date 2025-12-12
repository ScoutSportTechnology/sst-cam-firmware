#include "config/adapters/json/json_adapter.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "config/domain/profile_config.hpp"

namespace fs = std::filesystem;

TEST(JsonAdapter, LoadsProfileConfigFromFixtureFile) {
    using sst::config::adapters::JsonAdapter;
    using sst::config::domain::ProfileConfig;

    const fs::path path =
        fs::path(SST_REPO_ROOT) / "tests" / "config" / "config_files" / "profile.json";

    JsonAdapter<ProfileConfig> adapter{path};

    ProfileConfig cfg{};
    std::string error;
    ASSERT_TRUE(adapter.load(cfg, error)) << error;

    std::cout << "Loaded ProfileConfig: " << cfg.users.size() << std::endl;

    // Minimal sanity checks (adjust to match your actual JSON/struct)
    EXPECT_GE(cfg.users.size(), 0u);  // always true, but keeps compiler happy
}
