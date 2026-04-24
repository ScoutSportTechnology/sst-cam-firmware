#include <spdlog/spdlog.h>

#include "app/config/services/config_loader/config-loader.hpp"
#include "app/db/services/db_manager/db-manager.hpp"

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

    // Database
    sst::db::DbManager db({
        .db_path = sst::paths::kDbFile,
        .schema_path = sst::paths::kDbSchema,
    });

    // Config (user_id = 1 = admin, seeded by 001_defaults.sql)
    constexpr std::uint32_t kAdminUserId = 1;
    sst::config::app::ConfigLoader config(sst::paths::kConfigDir, sst::paths::kConfigFormat,
                                          kAdminUserId);
    [[maybe_unused]] auto cfg = config.get();

    spdlog::info("startup complete");
    return 0;
}
