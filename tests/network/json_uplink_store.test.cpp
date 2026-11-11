#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "../support/temp-dir.hpp"
#include "adapters/network/json/json-uplink-store.hpp"
#include "domain/config/models/uplink-config.hpp"

namespace {

namespace fs = std::filesystem;
using sst::test::TempDir;

// Persisting a config writes the file and restricts it to owner rw (0600), so
// the WiFi passphrase it contains is not world-readable.
TEST(JsonUplinkStoreTest, PersistsWithOwnerOnlyPermissions) {
    TempDir dir("uplink-perms");
    const fs::path path = dir.path() / "uplink-store-perms-test.json";

    sst::config::UplinkData data;
    data.wifi.enabled = true;
    data.wifi.ssid = "venue-net";
    data.wifi.passphrase = "super-secret";

    sst::adapters::network::JsonUplinkStore store(path.string());
    EXPECT_TRUE(store.Persist(data));
    ASSERT_TRUE(fs::exists(path));

    // Mode is exactly owner read/write (0600) — no group/other bits.
    const fs::perms mode = fs::status(path).permissions();
    EXPECT_EQ(mode & fs::perms::owner_read, fs::perms::owner_read);
    EXPECT_EQ(mode & fs::perms::owner_write, fs::perms::owner_write);
    EXPECT_EQ(mode & fs::perms::group_all, fs::perms::none);
    EXPECT_EQ(mode & fs::perms::others_all, fs::perms::none);
}

// A pre-existing world-readable file is tightened, not left at 0644.
TEST(JsonUplinkStoreTest, TightensPreexistingWorldReadableFile) {
    TempDir dir("uplink-pre");
    const fs::path path = dir.path() / "uplink-store-pre-test.json";
    {
        std::ofstream seed(path, std::ios::trunc);
        seed << "{}";
    }
    fs::permissions(path,
                    fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read |
                        fs::perms::others_read,
                    fs::perm_options::replace);

    sst::adapters::network::JsonUplinkStore store(path.string());
    EXPECT_TRUE(store.Persist(sst::config::UplinkData{}));

    const fs::perms mode = fs::status(path).permissions();
    EXPECT_EQ(mode & fs::perms::group_all, fs::perms::none);
    EXPECT_EQ(mode & fs::perms::others_all, fs::perms::none);
}

}  // namespace
