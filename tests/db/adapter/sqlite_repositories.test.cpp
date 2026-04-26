#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>

#include "app/db/services/db_manager/db-manager.hpp"
#include "domain/db/models/camera.hpp"
#include "domain/db/models/microphone.hpp"
#include "domain/db/models/network.hpp"
#include "domain/db/models/storage-config.hpp"
#include "domain/db/models/stream-config.hpp"
#include "domain/db/models/user.hpp"

namespace {

constexpr const char* kSchemaPath = SST_REPO_ROOT_DIR "/db/schema.sql";
constexpr int64_t kMissingId = 9999;

class DbTest : public ::testing::Test {
   protected:
    void SetUp() override {
        mgr_ = std::make_unique<sst::db::DbManager>(
            sst::db::DbManager::Config{.db_path = ":memory:", .schema_path = kSchemaPath});
        auto res = mgr_->users().create("testuser");
        ASSERT_TRUE(res.success);
        user_id_ = res.data.id;
    }

    std::unique_ptr<sst::db::DbManager> mgr_;
    int64_t user_id_{0};
};

// ── User ──────────────────────────────────────────────────────────────────────

TEST_F(DbTest, UserCreateAndGet) {
    auto res = mgr_->users().get(user_id_);
    ASSERT_TRUE(res.success);
    EXPECT_EQ(res.data.id, user_id_);
    EXPECT_EQ(res.data.username, "testuser");
}

TEST_F(DbTest, UserGetMissing) {
    EXPECT_FALSE(mgr_->users().get(kMissingId).success);
}

// ── Camera Config ─────────────────────────────────────────────────────────────

TEST_F(DbTest, CameraConfigRoundTrip) {
    using sst::db::CameraConfig;
    using sst::db::CameraFormat;
    using sst::db::CameraFocus;
    using sst::db::CameraWhiteBalance;

    constexpr int32_t kExposure = 200;
    constexpr float kGain = 2.5F;
    constexpr int32_t kWidth = 1280;
    constexpr int32_t kHeight = 720;
    constexpr int32_t kFps = 30;

    CameraConfig cfg{
        .user_id = user_id_,
        .exposure = kExposure,
        .gain = kGain,
        .white_balance = CameraWhiteBalance::kManual,
        .focus = CameraFocus::kManual,
        .width = kWidth,
        .height = kHeight,
        .format = CameraFormat::kNV12,
        .fps = kFps,
    };
    ASSERT_TRUE(mgr_->cameras().saveConfig(cfg).success);

    auto res = mgr_->cameras().getConfig(user_id_);
    ASSERT_TRUE(res.success);
    EXPECT_EQ(res.data.exposure, kExposure);
    EXPECT_FLOAT_EQ(res.data.gain, kGain);
    EXPECT_EQ(res.data.white_balance, CameraWhiteBalance::kManual);
    EXPECT_EQ(res.data.focus, CameraFocus::kManual);
    EXPECT_EQ(res.data.width, kWidth);
    EXPECT_EQ(res.data.height, kHeight);
    EXPECT_EQ(res.data.format, CameraFormat::kNV12);
    EXPECT_EQ(res.data.fps, kFps);
}

TEST_F(DbTest, CameraConfigUpdate) {
    using sst::db::CameraConfig;

    constexpr int32_t kUpdatedExposure = 300;

    CameraConfig cfg{.user_id = user_id_, .exposure = CameraConfig::kDefaultExposure};
    ASSERT_TRUE(mgr_->cameras().saveConfig(cfg).success);
    cfg.exposure = kUpdatedExposure;
    ASSERT_TRUE(mgr_->cameras().saveConfig(cfg).success);

    auto res = mgr_->cameras().getConfig(user_id_);
    ASSERT_TRUE(res.success);
    EXPECT_EQ(res.data.exposure, kUpdatedExposure);
}

TEST_F(DbTest, CameraConfigGetMissing) {
    EXPECT_FALSE(mgr_->cameras().getConfig(kMissingId).success);
}

// ── Camera Calibration ────────────────────────────────────────────────────────

TEST_F(DbTest, CameraCalibrationRoundTrip) {
    using sst::db::CameraCalibration;

    constexpr std::array<float, CameraCalibration::kIntrinsicMatrixSize> kIdentityMatrix = {
        1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    constexpr std::array<float, CameraCalibration::kDistortionCoefficientsSize> kDistortion = {
        0.1F, 0.2F, 0.3F, 0.4F, 0.5F};

    CameraCalibration cal;
    cal.camera_id = 0;
    cal.intrinsic_matrix = kIdentityMatrix;
    cal.distortion_coefficients = kDistortion;

    auto insert_res = mgr_->cameras().insertCalibration(cal);
    ASSERT_TRUE(insert_res.success);
    EXPECT_GT(insert_res.data.id, 0);

    auto get_res = mgr_->cameras().getLatestCalibration(0);
    ASSERT_TRUE(get_res.success);
    EXPECT_EQ(get_res.data.camera_id, 0);
    EXPECT_EQ(get_res.data.intrinsic_matrix, kIdentityMatrix);
    EXPECT_EQ(get_res.data.distortion_coefficients, kDistortion);
}

TEST_F(DbTest, CameraCalibrationGetMissing) {
    EXPECT_FALSE(mgr_->cameras().getLatestCalibration(0).success);
}

// ── Microphone Config ─────────────────────────────────────────────────────────

TEST_F(DbTest, MicrophoneConfigRoundTrip) {
    using sst::db::MicrophoneConfig;

    MicrophoneConfig cfg{.user_id = user_id_, .noise_reduction = false};
    ASSERT_TRUE(mgr_->microphones().saveConfig(cfg).success);

    auto res = mgr_->microphones().getConfig(user_id_);
    ASSERT_TRUE(res.success);
    EXPECT_EQ(res.data.user_id, user_id_);
    EXPECT_FALSE(res.data.noise_reduction);
}

TEST_F(DbTest, MicrophoneConfigUpdate) {
    using sst::db::MicrophoneConfig;

    MicrophoneConfig cfg{.user_id = user_id_, .noise_reduction = true};
    ASSERT_TRUE(mgr_->microphones().saveConfig(cfg).success);
    cfg.noise_reduction = false;
    ASSERT_TRUE(mgr_->microphones().saveConfig(cfg).success);

    auto res = mgr_->microphones().getConfig(user_id_);
    ASSERT_TRUE(res.success);
    EXPECT_FALSE(res.data.noise_reduction);
}

TEST_F(DbTest, MicrophoneConfigGetMissing) {
    EXPECT_FALSE(mgr_->microphones().getConfig(kMissingId).success);
}

TEST_F(DbTest, MicrophoneCalibrationRoundTrip) {
    using sst::db::MicrophoneCalibration;

    constexpr float kSensitivity = 1.5F;
    MicrophoneCalibration cal{.id = 0, .mic_id = 0, .sensitivity = kSensitivity, .calibrated_at = {}};
    auto insert_res = mgr_->microphones().insertCalibration(cal);
    ASSERT_TRUE(insert_res.success);
    EXPECT_GT(insert_res.data.id, 0);

    auto get_res = mgr_->microphones().getLatestCalibration(0);
    ASSERT_TRUE(get_res.success);
    EXPECT_EQ(get_res.data.mic_id, 0);
    EXPECT_FLOAT_EQ(get_res.data.sensitivity, kSensitivity);
    EXPECT_FALSE(get_res.data.calibrated_at.empty());
}

TEST_F(DbTest, MicrophoneCalibrationGetMissing) {
    EXPECT_FALSE(mgr_->microphones().getLatestCalibration(0).success);
}

// ── Network Client ────────────────────────────────────────────────────────────

TEST_F(DbTest, NetworkClientWifiRoundTrip) {
    using sst::db::NetworkClient;
    using sst::db::NetworkMedium;

    NetworkClient client{
        .user_id = user_id_,
        .enabled = true,
        .medium = NetworkMedium::kWifi,
        .ssid = "my-ssid",
        .wifi_password = "secret",
        .static_ip = false,
        .ip_address = std::nullopt,
        .subnet_mask = std::nullopt,
        .gateway = std::nullopt,
    };
    ASSERT_TRUE(mgr_->network().saveClient(client).success);

    auto res = mgr_->network().getClient(user_id_);
    ASSERT_TRUE(res.success);
    EXPECT_TRUE(res.data.enabled);
    EXPECT_EQ(res.data.medium, NetworkMedium::kWifi);
    ASSERT_TRUE(res.data.ssid.has_value());
    EXPECT_EQ(*res.data.ssid, "my-ssid");
    ASSERT_TRUE(res.data.wifi_password.has_value());
    EXPECT_EQ(*res.data.wifi_password, "secret");
    EXPECT_FALSE(res.data.static_ip);
}

TEST_F(DbTest, NetworkClientEthernet) {
    using sst::db::NetworkClient;
    using sst::db::NetworkMedium;

    // Ethernet: schema CHECK allows ssid/wifi_password to be null when medium=1
    NetworkClient client{
        .user_id = user_id_,
        .enabled = false,
        .medium = NetworkMedium::kEthernet,
        .ssid = std::nullopt,
        .wifi_password = std::nullopt,
        .static_ip = false,
        .ip_address = std::nullopt,
        .subnet_mask = std::nullopt,
        .gateway = std::nullopt,
    };
    ASSERT_TRUE(mgr_->network().saveClient(client).success);

    auto res = mgr_->network().getClient(user_id_);
    ASSERT_TRUE(res.success);
    EXPECT_EQ(res.data.medium, NetworkMedium::kEthernet);
    EXPECT_FALSE(res.data.ssid.has_value());
}

TEST_F(DbTest, NetworkClientGetMissing) {
    EXPECT_FALSE(mgr_->network().getClient(kMissingId).success);
}

// ── Network Access Point ──────────────────────────────────────────────────────

TEST_F(DbTest, NetworkAccessPointRoundTrip) {
    using sst::db::NetworkAccessPoint;
    using sst::db::NetworkMedium;

    NetworkAccessPoint access_point{
        .user_id = user_id_,
        .enabled = true,
        .medium = NetworkMedium::kWifi,
        .ssid = "sst-ap",
        .wifi_password = "appass",
        .static_ip = false,
        .ip_address = std::nullopt,
        .subnet_mask = std::nullopt,
        .gateway = std::nullopt,
    };
    ASSERT_TRUE(mgr_->network().saveAccessPoint(access_point).success);

    auto res = mgr_->network().getAccessPoint(user_id_);
    ASSERT_TRUE(res.success);
    EXPECT_TRUE(res.data.enabled);
    ASSERT_TRUE(res.data.ssid.has_value());
    EXPECT_EQ(*res.data.ssid, "sst-ap");
    ASSERT_TRUE(res.data.wifi_password.has_value());
    EXPECT_EQ(*res.data.wifi_password, "appass");
}

TEST_F(DbTest, NetworkAccessPointGetMissing) {
    EXPECT_FALSE(mgr_->network().getAccessPoint(kMissingId).success);
}

// ── Network Bluetooth ─────────────────────────────────────────────────────────

TEST_F(DbTest, NetworkBluetoothRoundTrip) {
    using sst::db::NetworkBluetooth;

    NetworkBluetooth bluetooth{
        .user_id = user_id_,
        .enabled = true,
        .name = "sst-cam",
        .password = "btpass",
    };
    ASSERT_TRUE(mgr_->network().saveBluetooth(bluetooth).success);

    auto res = mgr_->network().getBluetooth(user_id_);
    ASSERT_TRUE(res.success);
    EXPECT_TRUE(res.data.enabled);
    EXPECT_EQ(res.data.name, "sst-cam");
    EXPECT_EQ(res.data.password, "btpass");
}

TEST_F(DbTest, NetworkBluetoothGetMissing) {
    EXPECT_FALSE(mgr_->network().getBluetooth(kMissingId).success);
}

// ── Storage Config ────────────────────────────────────────────────────────────

TEST_F(DbTest, StorageConfigGetAllEmpty) {
    auto res = mgr_->storage().getAll(user_id_);
    ASSERT_TRUE(res.success);
    EXPECT_TRUE(res.data.empty());
}

TEST_F(DbTest, StorageConfigRoundTrip) {
    using sst::db::StorageConfig;
    using sst::db::StorageFormat;
    using sst::db::StorageType;

    StorageConfig cfg{
        .user_id = user_id_,
        .type = StorageType::kRecording,
        .enabled = true,
        .format = StorageFormat::kMp4,
        .path = "/var/lib/sst/recording",
    };
    ASSERT_TRUE(mgr_->storage().save(cfg).success);

    auto res = mgr_->storage().getAll(user_id_);
    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.data.size(), 1U);
    EXPECT_EQ(res.data[0].type, StorageType::kRecording);
    EXPECT_EQ(res.data[0].format, StorageFormat::kMp4);
    EXPECT_EQ(res.data[0].path, "/var/lib/sst/recording");
}

TEST_F(DbTest, StorageConfigAllFourTypes) {
    using sst::db::StorageConfig;
    using sst::db::StorageFormat;
    using sst::db::StorageType;

    const std::array<StorageConfig, 4> configs{
        StorageConfig{.user_id = user_id_, .type = StorageType::kLogs, .enabled = true, .format = StorageFormat::kTxt, .path = "/var/lib/sst/logs"},
        StorageConfig{.user_id = user_id_, .type = StorageType::kRecording, .enabled = true, .format = StorageFormat::kMp4, .path = "/var/lib/sst/rec"},
        StorageConfig{.user_id = user_id_, .type = StorageType::kSnapshots, .enabled = true, .format = StorageFormat::kJpg, .path = "/var/lib/sst/snap"},
        StorageConfig{.user_id = user_id_, .type = StorageType::kThumbnails, .enabled = true, .format = StorageFormat::kJpg, .path = "/var/lib/sst/thumb"},
    };
    for (const auto& cfg : configs) {
        ASSERT_TRUE(mgr_->storage().save(cfg).success);
    }

    constexpr std::size_t kAllStorageTypes = 4U;
    auto res = mgr_->storage().getAll(user_id_);
    ASSERT_TRUE(res.success);
    EXPECT_EQ(res.data.size(), kAllStorageTypes);
}

TEST_F(DbTest, StorageConfigUpdate) {
    using sst::db::StorageConfig;
    using sst::db::StorageFormat;
    using sst::db::StorageType;

    StorageConfig cfg{.user_id = user_id_, .type = StorageType::kLogs, .enabled = true, .format = StorageFormat::kTxt, .path = "/old/path"};
    ASSERT_TRUE(mgr_->storage().save(cfg).success);
    cfg.path = "/new/path";
    ASSERT_TRUE(mgr_->storage().save(cfg).success);

    auto res = mgr_->storage().getAll(user_id_);
    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.data.size(), 1U);
    EXPECT_EQ(res.data[0].path, "/new/path");
}

// ── Stream Config ─────────────────────────────────────────────────────────────

TEST_F(DbTest, StreamConfigGetAllEmpty) {
    auto res = mgr_->streams().getAll(user_id_);
    ASSERT_TRUE(res.success);
    EXPECT_TRUE(res.data.empty());
}

TEST_F(DbTest, StreamConfigInsertAndGet) {
    using sst::db::StreamCodec;
    using sst::db::StreamConfig;
    using sst::db::StreamPlatform;
    using sst::db::StreamType;

    StreamConfig cfg{
        .id = 0,
        .user_id = user_id_,
        .platform = StreamPlatform::kCustom,
        .name = "My Stream",
        .enabled = true,
        .stream_key = "abc123",
        .stream_type = StreamType::kRtmp,
        .url = "rtmp://example.com/live",
        .codec = StreamCodec::kH264,
        .width = StreamConfig::kDefaultWidth,
        .height = StreamConfig::kDefaultHeight,
        .framerate = StreamConfig::kDefaultFramerate,
        .bitrate_kbps = StreamConfig::kDefaultBitrateKbps,
    };
    auto save_res = mgr_->streams().save(cfg);
    ASSERT_TRUE(save_res.success);
    EXPECT_GT(save_res.data.id, 0);

    auto get_res = mgr_->streams().getAll(user_id_);
    ASSERT_TRUE(get_res.success);
    ASSERT_EQ(get_res.data.size(), 1U);
    EXPECT_EQ(get_res.data[0].name, "My Stream");
    EXPECT_EQ(get_res.data[0].stream_key, "abc123");
    EXPECT_TRUE(get_res.data[0].enabled);
    EXPECT_EQ(get_res.data[0].codec, StreamCodec::kH264);
    EXPECT_EQ(get_res.data[0].platform, StreamPlatform::kCustom);
}

TEST_F(DbTest, StreamConfigUpdate) {
    using sst::db::StreamCodec;
    using sst::db::StreamConfig;
    using sst::db::StreamPlatform;
    using sst::db::StreamType;

    constexpr int32_t kInitialBitrate = 6000;
    constexpr int32_t kUpdatedBitrate = 8000;

    StreamConfig cfg{
        .id = 0,
        .user_id = user_id_,
        .platform = StreamPlatform::kYouTube,
        .name = "YT Stream",
        .enabled = false,
        .stream_key = "yt-key",
        .stream_type = StreamType::kRtmps,
        .url = "rtmps://youtube.com/live",
        .codec = StreamCodec::kH265,
        .width = StreamConfig::kDefaultWidth,
        .height = StreamConfig::kDefaultHeight,
        .framerate = StreamConfig::kDefaultFramerate,
        .bitrate_kbps = kInitialBitrate,
    };
    auto insert_res = mgr_->streams().save(cfg);
    ASSERT_TRUE(insert_res.success);

    StreamConfig updated = insert_res.data;
    updated.enabled = true;
    updated.bitrate_kbps = kUpdatedBitrate;
    ASSERT_TRUE(mgr_->streams().save(updated).success);

    auto get_res = mgr_->streams().getAll(user_id_);
    ASSERT_TRUE(get_res.success);
    ASSERT_EQ(get_res.data.size(), 1U);
    EXPECT_TRUE(get_res.data[0].enabled);
    EXPECT_EQ(get_res.data[0].bitrate_kbps, kUpdatedBitrate);
}

TEST_F(DbTest, StreamConfigRemove) {
    using sst::db::StreamCodec;
    using sst::db::StreamConfig;
    using sst::db::StreamPlatform;
    using sst::db::StreamType;

    constexpr int32_t kSdWidth = 1280;
    constexpr int32_t kSdHeight = 720;
    constexpr int32_t kSdFramerate = 30;
    constexpr int32_t kSdBitrate = 3000;

    StreamConfig cfg{
        .id = 0,
        .user_id = user_id_,
        .platform = StreamPlatform::kTwitch,
        .name = "Twitch",
        .enabled = false,
        .stream_key = "twitch-key",
        .stream_type = StreamType::kRtmp,
        .url = "rtmp://twitch.tv/live",
        .codec = StreamCodec::kH264,
        .width = kSdWidth,
        .height = kSdHeight,
        .framerate = kSdFramerate,
        .bitrate_kbps = kSdBitrate,
    };
    auto insert_res = mgr_->streams().save(cfg);
    ASSERT_TRUE(insert_res.success);

    auto remove_res = mgr_->streams().remove(insert_res.data.id);
    ASSERT_TRUE(remove_res.success);
    EXPECT_TRUE(remove_res.data);

    auto get_res = mgr_->streams().getAll(user_id_);
    ASSERT_TRUE(get_res.success);
    EXPECT_TRUE(get_res.data.empty());
}

TEST_F(DbTest, StreamConfigRemoveNonExistent) {
    auto res = mgr_->streams().remove(kMissingId);
    ASSERT_TRUE(res.success);
    EXPECT_FALSE(res.data);
}

}  // namespace
