#include "adapters/db/sqlite/storage-config-repository.hpp"

#include <spdlog/spdlog.h>

namespace sst::db {

SqliteStorageConfigRepository::SqliteStorageConfigRepository(DbConnection& conn)
    : db_(conn.db()) {}

auto SqliteStorageConfigRepository::getAll(int64_t user_id)
    -> DbResult<std::vector<StorageConfig>> {
    try {
        SQLite::Statement stmt(
            db_,
            "SELECT user_id, type, enabled, format, path "
            "FROM storage_config WHERE user_id = ?");
        stmt.bind(1, user_id);
        std::vector<StorageConfig> results;
        while (stmt.executeStep()) {
            int col = 0;
            StorageConfig cfg;
            cfg.user_id = stmt.getColumn(col++).getInt64();
            cfg.type = static_cast<StorageType>(stmt.getColumn(col++).getInt());
            cfg.enabled = static_cast<bool>(stmt.getColumn(col++).getInt());
            cfg.format = static_cast<StorageFormat>(stmt.getColumn(col++).getInt());
            cfg.path = stmt.getColumn(col).getText();
            results.push_back(std::move(cfg));
        }
        return DbResult<std::vector<StorageConfig>>::ok(std::move(results));
    } catch (const SQLite::Exception& ex) {
        spdlog::error("StorageConfigRepository::getAll failed: {}", ex.what());
        return DbResult<std::vector<StorageConfig>>::fail();
    }
}

auto SqliteStorageConfigRepository::save(const StorageConfig& data) -> DbResult<StorageConfig> {
    try {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO storage_config (user_id, type, enabled, format, path) "
            "VALUES (?, ?, ?, ?, ?)");
        int param = 1;
        stmt.bind(param++, data.user_id);
        stmt.bind(param++, static_cast<int>(data.type));
        stmt.bind(param++, static_cast<int>(data.enabled));
        stmt.bind(param++, static_cast<int>(data.format));
        stmt.bind(param, data.path);
        stmt.exec();
        return DbResult<StorageConfig>::ok(data);
    } catch (const SQLite::Exception& ex) {
        spdlog::error("StorageConfigRepository::save failed: {}", ex.what());
        return DbResult<StorageConfig>::fail();
    }
}

}  // namespace sst::db
