#include <memory>

#include <spdlog/spdlog.h>

#include "adapters/control/ble/bluez/bluez-ble-transport.hpp"
#include "adapters/control/wifi/wpa_supplicant/wpa-wifi-manager.hpp"
#include "adapters/streaming/gst_rtsp/gst-rtsp-preview-server.hpp"
#include "app/config/services/config_loader/config-loader.hpp"
#include "app/control/services/control_module/control-module.hpp"
#include "app/control/services/controllers/camera.controller.hpp"
#include "app/control/services/controllers/network.controller.hpp"
#include "app/control/services/controllers/recording.controller.hpp"
#include "app/control/services/controllers/streaming.controller.hpp"
#include "app/control/services/controllers/system.controller.hpp"
#include "app/db/services/db_manager/db-manager.hpp"
#include "app/db/services/db_seeder/db-seeder.hpp"

// ============================================================
// Runtime paths — adjust per deployment environment
// ============================================================
namespace sst::paths {

constexpr const char* kDbFile = "/var/lib/sst/cam/data.db";
constexpr const char* kDbSchema = "/usr/share/sst/cam/schema.sql";

constexpr const char* kConfigDir = "/etc/sst/cam/config";
constexpr const char* kConfigFormat = "json";

}  // namespace sst::paths

auto main() -> int {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("sst-cam-firmware starting");

    // Config module: static device info + initial calibration seeds + storage paths
    constexpr std::uint32_t kDefaultUserId = 1;
    sst::config::app::ConfigLoader config(sst::paths::kConfigDir, sst::paths::kConfigFormat);
    auto cfg = config.get();

    // Database
    sst::db::DbManager database({
        .db_path = sst::paths::kDbFile,
        .schema_path = sst::paths::kDbSchema,
    });

    // Seed DB from config on first boot (idempotent)
    sst::db::DbSeeder::seedIfEmpty(database, cfg);

    // Device model — stays in config module (device info)
    [[maybe_unused]] const std::string device_model = cfg.device.model.value_or("");

    // Camera settings — from DB from here on
    [[maybe_unused]] auto camera0_cfg = database.cameras().getConfig(kDefaultUserId);
    // sst::capture::GStreamerAdapter cam0(camera0_cfg.data, device_model, 0);

    // ============================================================
    // Control plane (BLE commands + WiFi-Direct bring-up) and the
    // live-preview RTSP server (data plane). Adapters are stubs today —
    // see TODO blocks in adapters/control/* and adapters/streaming/* for the
    // BlueZ / wpa_supplicant / gst-rtsp wiring still to be filled in.
    // ============================================================
    auto ble_transport = std::make_unique<sst::adapters::control::BluezBleTransport>();
    auto wifi_manager  = std::make_unique<sst::adapters::control::WpaWifiManager>("wlan0");

    sst::control::ControlModule control(std::move(ble_transport), std::move(wifi_manager));

    auto preview_server = std::make_unique<sst::adapters::streaming::GstRtspPreviewServer>();

    // ★ Extensibility point — register one IController per concern.
    //   Add a new controllers/<thing>.controller.{hpp,cpp} and Register() it here.
    control.ble().Register(std::make_shared<sst::control::NetworkController>(
        control.wifi(), database.network(), kDefaultUserId));
    control.ble().Register(std::make_shared<sst::control::StreamingController>(*preview_server));
    control.ble().Register(std::make_shared<sst::control::CameraController>());
    control.ble().Register(std::make_shared<sst::control::RecordingController>());
    control.ble().Register(std::make_shared<sst::control::SystemController>());

    control.Start();

    spdlog::info("startup complete");
    return 0;
}
