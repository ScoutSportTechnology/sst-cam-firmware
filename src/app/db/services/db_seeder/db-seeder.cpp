#include "app/db/services/db_seeder/db-seeder.hpp"

#include <spdlog/spdlog.h>

#include "domain/config/models/calibration.hpp"
#include "domain/config/models/wifi-direct.hpp"
#include "domain/db/models/camera.hpp"
#include "domain/db/models/microphone.hpp"
#include "domain/db/models/network.hpp"

namespace sst::db {

static constexpr int64_t kDefaultUserId = 1;
static constexpr const char* kDefaultUsername = "Default";
static constexpr int32_t kDefaultWifiDirectChannel = 6;

static void seedCameraConfig(DbManager& mgr, const sst::config::CalibrationCamerasData& src) {
    CameraConfig cfg;
    cfg.user_id = kDefaultUserId;
    cfg.exposure = static_cast<int32_t>(src.exposure.value_or(CameraConfig::kDefaultExposure));
    cfg.gain = src.gain.value_or(CameraConfig::kDefaultGain);
    cfg.white_balance = static_cast<CameraWhiteBalance>(src.white_balance.value_or(0));
    cfg.focus = static_cast<CameraFocus>(src.focus.value_or(0));
    cfg.width = static_cast<int32_t>(src.width.value_or(CameraConfig::kDefaultWidth));
    cfg.height = static_cast<int32_t>(src.height.value_or(CameraConfig::kDefaultHeight));
    cfg.format = static_cast<sst::common::PixelFormat>(src.format.value_or(0));
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
    cfg.user_id = kDefaultUserId;
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

static void seedWifiDirect(DbManager& mgr, const sst::config::WifiDirectData& src) {
    if (!src.ssid || !src.passphrase) {
        spdlog::error(
            "DbSeeder: wifi-direct.json missing required ssid/passphrase — "
            "the BLE bootstrap_p2p flow will fail until this is fixed");
        return;
    }
    NetworkWifiDirect row;
    row.user_id = kDefaultUserId;
    row.enabled = src.enabled.value_or(true);
    row.ssid = *src.ssid;
    row.passphrase = *src.passphrase;
    row.channel = static_cast<int32_t>(src.channel.value_or(kDefaultWifiDirectChannel));
    row.ip_address = src.ip_address;
    mgr.network().saveWifiDirect(row);
}

static void seedBluetoothDefaults(DbManager& mgr) {
    mgr.network().saveBluetooth(
        {.user_id = kDefaultUserId, .enabled = true, .name = "sst-cam", .password = "123456"});
}

void DbSeeder::seedIfEmpty(DbManager& mgr, const sst::config::ConfigData& config) {
    auto existing = mgr.users().get(kDefaultUserId);
    if (existing.success) {
        spdlog::debug("DB already seeded, skipping");
        return;
    }

    spdlog::info("Seeding DB with initial data");

    if (!mgr.users().create(kDefaultUsername).success) {
        spdlog::error("DB seeding aborted: failed to create default user");
        return;
    }

    const auto cams =
        config.calibration.cameras.value_or(sst::config::CalibrationCamerasData{});
    seedCameraConfig(mgr, cams);
    seedCameraCalibrations(mgr, cams);

    const auto mics =
        config.calibration.microphones.value_or(sst::config::CalibrationMicrophonesData{});
    seedMicrophoneConfig(mgr, mics);
    seedMicrophoneCalibrations(mgr, mics);

    seedWifiDirect(mgr, config.wifi_direct);
    seedBluetoothDefaults(mgr);

    spdlog::info("DB seeding complete");
}

}  // namespace sst::db
