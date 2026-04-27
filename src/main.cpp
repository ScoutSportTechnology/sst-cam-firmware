#include <spdlog/spdlog.h>

#include "app/config/services/config_loader/config-loader.hpp"
#include "app/db/services/db_manager/db-manager.hpp"
#include "app/db/services/db_seeder/db-seeder.hpp"
#include "domain/config/models/device-model.hpp"

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
    [[maybe_unused]] sst::config::DeviceModel device_model = sst::config::DeviceModel::UNKNOWN;
    if (cfg.device.model) {
        device_model = sst::config::FromString(*cfg.device.model);
    }

    // Camera settings — from DB from here on
    [[maybe_unused]] auto camera0_cfg = database.cameras().getConfig(kDefaultUserId);
    // sst::capture::GStreamerAdapter cam0(camera0_cfg.data, device_model, 0);

    spdlog::info("startup complete");
    return 0;
}
