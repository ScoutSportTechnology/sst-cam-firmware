#include "config/adapters/json/json_adapter.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "config/domain/profile_config.hpp"

namespace fs = std::filesystem;

TEST(JsonAdapter, LoadsProfileConfigFromJsonFile) {
    using sst::config::adapters::JsonAdapter;
    using sst::config::domain::ProfileConfig;

    // Arrange: create temp file
    const fs::path path = fs::temp_directory_path() / "profile_config_test.json";

    const char* json_text = R"json(
    {
      "default_data": { "calibration": 0, "device": 0, "stream": 0, "storage": 0 },
      "users": [
        { "id": 0, "user_data": { "device": 1 } }
      ]
    }
    )json";

    {
        std::ofstream out(path);
        ASSERT_TRUE(out.good());
        out << json_text;
    }

    // Act
    JsonAdapter<ProfileConfig> adapter{path};

    ProfileConfig cfg{};
    std::string error;
    ASSERT_TRUE(adapter.load(cfg, error)) << error;
    EXPECT_TRUE(error.empty());

    // Assert (instead of printing)
    EXPECT_EQ(cfg.default_data.calibration, 0);
    EXPECT_EQ(cfg.default_data.device, 0);
    EXPECT_EQ(cfg.default_data.stream, 0);
    EXPECT_EQ(cfg.default_data.storage, 0);

    ASSERT_EQ(cfg.users.size(), 1u);
    EXPECT_EQ(cfg.users[0].id, 0);

    ASSERT_TRUE(cfg.users[0].user_data.device.has_value());
    EXPECT_EQ(*cfg.users[0].user_data.device, 1);

    // Cleanup
    std::error_code ec;
    fs::remove(path, ec);
}
