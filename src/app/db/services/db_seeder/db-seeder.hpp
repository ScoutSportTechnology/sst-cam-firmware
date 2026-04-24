#pragma once

#include "app/db/services/db_manager/db-manager.hpp"
#include "domain/config/models/config-data.hpp"

namespace sst::db {

class DbSeeder {
   public:
    // Seeds all tables if DB is empty (idempotent — no-op if admin user exists).
    static void seedIfEmpty(DbManager& mgr, const sst::config::ConfigData& config);
};

}  // namespace sst::db
