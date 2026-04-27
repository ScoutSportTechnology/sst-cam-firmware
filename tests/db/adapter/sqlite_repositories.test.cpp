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
#include "domain/db/models/formatter/_fmt.hpp"  // IWYU pragma: keep

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
    auto known = mgr_->users().get(kDefaultUserId);
    ASSERT_TRUE(known.success);
    spdlog::info("User:\n{}", known.data);
    EXPECT_FALSE(mgr_->users().get(kMissingUserId).success);
}

TEST_F(DbTest, CameraRead) {
    spdlog::info("CameraRead: known config/calibration + missing ids");
    auto cfg = mgr_->cameras().getConfig(kDefaultUserId);
    ASSERT_TRUE(cfg.success);
    spdlog::info("CameraConfig:\n{}", cfg.data);
    EXPECT_FALSE(mgr_->cameras().getConfig(kMissingUserId).success);

    auto cal = mgr_->cameras().getLatestCalibration(kSeededDeviceId);
    ASSERT_TRUE(cal.success);
    spdlog::info("CameraCalibration:\n{}", cal.data);
    EXPECT_FALSE(mgr_->cameras().getLatestCalibration(kMissingDeviceId).success);
}

TEST_F(DbTest, MicrophoneRead) {
    spdlog::info("MicrophoneRead: known config/calibration + missing ids");
    auto cfg = mgr_->microphones().getConfig(kDefaultUserId);
    ASSERT_TRUE(cfg.success);
    spdlog::info("MicrophoneConfig:\n{}", cfg.data);
    EXPECT_FALSE(mgr_->microphones().getConfig(kMissingUserId).success);

    auto cal = mgr_->microphones().getLatestCalibration(kSeededDeviceId);
    ASSERT_TRUE(cal.success);
    spdlog::info("MicrophoneCalibration:\n{}", cal.data);
    EXPECT_FALSE(mgr_->microphones().getLatestCalibration(kMissingDeviceId).success);
}

TEST_F(DbTest, NetworkRead) {
    spdlog::info("NetworkRead: known client/ap/bluetooth + missing user");
    auto client = mgr_->network().getClient(kDefaultUserId);
    ASSERT_TRUE(client.success);
    spdlog::info("NetworkClient:\n{}", client.data);
    EXPECT_FALSE(mgr_->network().getClient(kMissingUserId).success);

    auto access_point = mgr_->network().getAccessPoint(kDefaultUserId);
    ASSERT_TRUE(access_point.success);
    spdlog::info("NetworkAccessPoint:\n{}", access_point.data);
    EXPECT_FALSE(mgr_->network().getAccessPoint(kMissingUserId).success);

    auto bluetooth = mgr_->network().getBluetooth(kDefaultUserId);
    ASSERT_TRUE(bluetooth.success);
    spdlog::info("NetworkBluetooth:\n{}", bluetooth.data);
    EXPECT_FALSE(mgr_->network().getBluetooth(kMissingUserId).success);
}

TEST_F(DbTest, StreamRead) {
    spdlog::info("StreamRead: streams are not seeded, expect empty result");
    auto res = mgr_->streams().getAll(kDefaultUserId);
    ASSERT_TRUE(res.success);
    EXPECT_TRUE(res.data.empty());
}

}  // namespace
