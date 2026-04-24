#include "app/db/services/db_seeder/db-seeder.hpp"

#include <spdlog/spdlog.h>

#include <array>

#include "domain/config/models/calibration.hpp"
#include "domain/db/models/camera.hpp"
#include "domain/db/models/microphone.hpp"
#include "domain/db/models/network.hpp"
#include "domain/db/models/storage-config.hpp"

namespace sst::db {

static constexpr int64_t kAdminUserId = 1;

static void seedCameraConfig(DbManager& mgr, const sst::config::CalibrationCamerasData& src) {
    CameraConfig cfg;
    cfg.user_id = kAdminUserId;
    cfg.exposure = static_cast<int32_t>(src.exposure.value_or(CameraConfig::kDefaultExposure));
    cfg.gain = src.gain.value_or(CameraConfig::kDefaultGain);
    cfg.white_balance = static_cast<CameraWhiteBalance>(src.white_balance.value_or(0));
    cfg.focus = static_cast<CameraFocus>(src.focus.value_or(0));
    cfg.width = static_cast<int32_t>(src.width.value_or(CameraConfig::kDefaultWidth));
    cfg.height = static_cast<int32_t>(src.height.value_or(CameraConfig::kDefaultHeight));
    cfg.format = static_cast<CameraFormat>(src.format.value_or(0));
    cfg.fps = static_cast<int32_t>(src.fps.value_or(CameraConfig::kDefaultFps));
    mgr.cameras().saveConfig(cfg);
}

static void seedCameraCalibrations(DbManager& mgr,
                                   const sst::config::CalibrationCamerasData& src) {
    if (!src.device) {
        return;
    }
    for (const auto& dev : *src.device) {
        if (!dev.id || !dev.intrinsic_matrix || !dev.distortion_coefficients) {
            continue;
        }
        CameraCalibration cal;
        cal.camera_id = static_cast<int32_t>(*dev.id);
        cal.intrinsic_matrix = *dev.intrinsic_matrix;
        cal.distortion_coefficients = *dev.distortion_coefficients;
        mgr.cameras().insertCalibration(cal);
    }
}

static void seedMicrophoneConfig(DbManager& mgr,
                                 const sst::config::CalibrationMicrophonesData& src) {
    MicrophoneConfig cfg;
    cfg.user_id = kAdminUserId;
    cfg.noise_reduction = src.noise_reduction.value_or(true);
    mgr.microphones().saveConfig(cfg);
}

static void seedMicrophoneCalibrations(DbManager& mgr,
                                       const sst::config::CalibrationMicrophonesData& src) {
    if (!src.device) {
        return;
    }
    for (const auto& dev : *src.device) {
        if (!dev.id) {
            continue;
        }
        MicrophoneCalibration cal;
        cal.mic_id = static_cast<int32_t>(*dev.id);
        cal.sensitivity = dev.sensitivity.value_or(1.0F);
        mgr.microphones().insertCalibration(cal);
    }
}

static void seedNetworkDefaults(DbManager& mgr) {
    mgr.network().saveClient({.user_id = kAdminUserId,
                              .enabled = false,
                              .medium = NetworkMedium::kWifi,
                              .ssid = std::nullopt,
                              .wifi_password = std::nullopt,
                              .static_ip = false,
                              .ip_address = std::nullopt,
                              .subnet_mask = std::nullopt,
                              .gateway = std::nullopt});

    mgr.network().saveAccessPoint({.user_id = kAdminUserId,
                                   .enabled = false,
                                   .medium = NetworkMedium::kWifi,
                                   .ssid = "sst-cam",
                                   .wifi_password = "123456",
                                   .static_ip = false,
                                   .ip_address = std::nullopt,
                                   .subnet_mask = std::nullopt,
                                   .gateway = std::nullopt});

    mgr.network().saveBluetooth(
        {.user_id = kAdminUserId, .enabled = true, .name = "sst-cam", .password = "123456"});
}

static void seedStorageDefaults(DbManager& mgr) {
    const std::array<StorageConfig, 4> configs = {{
        {.user_id = kAdminUserId, .type = StorageType::kLogs, .enabled = true,
         .format = StorageFormat::kTxt, .path = "/var/log/sst/cam/"},
        {.user_id = kAdminUserId, .type = StorageType::kRecording, .enabled = true,
         .format = StorageFormat::kMp4, .path = "/var/lib/sst/cam/videos/"},
        {.user_id = kAdminUserId, .type = StorageType::kSnapshots, .enabled = true,
         .format = StorageFormat::kJpg, .path = "/var/lib/sst/cam/snapshots/"},
        {.user_id = kAdminUserId, .type = StorageType::kThumbnails, .enabled = true,
         .format = StorageFormat::kJpg, .path = "/var/lib/sst/cam/thumbnails/"},
    }};
    for (const auto& cfg : configs) {
        mgr.storage().save(cfg);
    }
}

void DbSeeder::seedIfEmpty(DbManager& mgr, const sst::config::ConfigData& config) {
    auto existing = mgr.users().get(kAdminUserId);
    if (existing.success) {
        spdlog::debug("DB already seeded, skipping");
        return;
    }

    spdlog::info("Seeding DB with initial data");

    if (!mgr.users().create("admin").success) {
        spdlog::error("DB seeding aborted: failed to create admin user");
        return;
    }

    if (config.calibration.cameras) {
        seedCameraConfig(mgr, *config.calibration.cameras);
        seedCameraCalibrations(mgr, *config.calibration.cameras);
    }
    if (config.calibration.microphones) {
        seedMicrophoneConfig(mgr, *config.calibration.microphones);
        seedMicrophoneCalibrations(mgr, *config.calibration.microphones);
    }

    seedNetworkDefaults(mgr);
    seedStorageDefaults(mgr);

    spdlog::info("DB seeding complete");
}

}  // namespace sst::db
