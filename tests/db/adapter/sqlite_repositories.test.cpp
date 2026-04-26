#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <array>
#include <filesystem>
#include <memory>
#include <vector>

#include "app/db/services/db_manager/db-manager.hpp"
#include "app/db/services/db_seeder/db-seeder.hpp"
#include "domain/config/models/calibration.hpp"
#include "domain/config/models/config-data.hpp"

namespace {

constexpr const char* kSchemaPath = SST_REPO_ROOT_DIR "/db/schema.sql";
constexpr const char* kDbPath = SST_REPO_ROOT_DIR "/tests/db/data/test.db";
constexpr int64_t kDefaultUserId = 1;
constexpr int64_t kMissingUserId = 9999;
constexpr int32_t kSeededDeviceId = 0;
constexpr int32_t kMissingDeviceId = 99;

auto MakeSeedConfig() -> sst::config::ConfigData {
    sst::config::CalibrationCameraDeviceData cam_dev;
    cam_dev.id = 0;
    cam_dev.intrinsic_matrix = std::array<float, 9>{1.F, 0.F, 0.F, 0.F, 1.F, 0.F, 0.F, 0.F, 1.F};
    cam_dev.distortion_coefficients = std::array<float, 5>{0.F, 0.F, 0.F, 0.F, 0.F};

    sst::config::CalibrationCamerasData cams;
    cams.device = std::vector{cam_dev};

    sst::config::CalibrationMicrophoneDeviceData mic_dev;
    mic_dev.id = 0;
    mic_dev.sensitivity = 1.F;

    sst::config::CalibrationMicrophonesData mics;
    mics.device = std::vector{mic_dev};

    sst::config::ConfigData cfg;
    cfg.calibration.cameras = cams;
    cfg.calibration.microphones = mics;
    return cfg;
}

class DbTest : public ::testing::Test {
   protected:
    auto SetUp() -> void override {
        const std::filesystem::path db_path{kDbPath};
        std::filesystem::create_directories(db_path.parent_path());
        std::filesystem::remove(db_path);
        spdlog::info("DbTest: starting fresh DB at {}", kDbPath);

        mgr_ = std::make_unique<sst::db::DbManager>(
            sst::db::DbManager::Config{.db_path = kDbPath, .schema_path = kSchemaPath});

        // Seeding is the write path under test — every read test below depends on it.
        sst::db::DbSeeder::seedIfEmpty(*mgr_, MakeSeedConfig());
        spdlog::info("DbTest: seed complete, ready for reads");
    }

    std::unique_ptr<sst::db::DbManager> mgr_;
};

TEST_F(DbTest, UserRead) {
    spdlog::info("UserRead: known admin user + missing user");
    EXPECT_TRUE(mgr_->users().get(kDefaultUserId).success);
    EXPECT_FALSE(mgr_->users().get(kMissingUserId).success);
}

TEST_F(DbTest, CameraRead) {
    spdlog::info("CameraRead: known config/calibration + missing ids");
    EXPECT_TRUE(mgr_->cameras().getConfig(kDefaultUserId).success);
    EXPECT_FALSE(mgr_->cameras().getConfig(kMissingUserId).success);
    EXPECT_TRUE(mgr_->cameras().getLatestCalibration(kSeededDeviceId).success);
    EXPECT_FALSE(mgr_->cameras().getLatestCalibration(kMissingDeviceId).success);
}

TEST_F(DbTest, MicrophoneRead) {
    spdlog::info("MicrophoneRead: known config/calibration + missing ids");
    EXPECT_TRUE(mgr_->microphones().getConfig(kDefaultUserId).success);
    EXPECT_FALSE(mgr_->microphones().getConfig(kMissingUserId).success);
    EXPECT_TRUE(mgr_->microphones().getLatestCalibration(kSeededDeviceId).success);
    EXPECT_FALSE(mgr_->microphones().getLatestCalibration(kMissingDeviceId).success);
}

TEST_F(DbTest, NetworkRead) {
    spdlog::info("NetworkRead: known client/ap/bluetooth + missing user");
    EXPECT_TRUE(mgr_->network().getClient(kDefaultUserId).success);
    EXPECT_FALSE(mgr_->network().getClient(kMissingUserId).success);
    EXPECT_TRUE(mgr_->network().getAccessPoint(kDefaultUserId).success);
    EXPECT_FALSE(mgr_->network().getAccessPoint(kMissingUserId).success);
    EXPECT_TRUE(mgr_->network().getBluetooth(kDefaultUserId).success);
    EXPECT_FALSE(mgr_->network().getBluetooth(kMissingUserId).success);
}

TEST_F(DbTest, StorageRead) {
    spdlog::info("StorageRead: 4 seeded rows for admin, empty for missing user");
    constexpr std::size_t kSeededStorageCount = 4U;

    auto seeded = mgr_->storage().getAll(kDefaultUserId);
    ASSERT_TRUE(seeded.success);
    EXPECT_EQ(seeded.data.size(), kSeededStorageCount);

    auto missing = mgr_->storage().getAll(kMissingUserId);
    ASSERT_TRUE(missing.success);
    EXPECT_TRUE(missing.data.empty());
}

TEST_F(DbTest, StreamRead) {
    spdlog::info("StreamRead: streams are not seeded, expect empty result");
    auto res = mgr_->streams().getAll(kDefaultUserId);
    ASSERT_TRUE(res.success);
    EXPECT_TRUE(res.data.empty());
}

}  // namespace
