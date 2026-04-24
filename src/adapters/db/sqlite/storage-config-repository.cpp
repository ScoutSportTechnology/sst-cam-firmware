#include "adapters/db/sqlite/storage-config-repository.hpp"

namespace sst::db {

SqliteStorageConfigRepository::SqliteStorageConfigRepository(DbConnection& conn)
    : SqliteRepositoryBase(conn) {}

auto SqliteStorageConfigRepository::getAll(int64_t user_id)
    -> DbResult<std::vector<StorageConfig>> {
    return dbExecute<std::vector<StorageConfig>>("StorageConfigRepository::getAll", [&] {
        SQLite::Statement stmt(
            db_,
            "SELECT user_id, type, enabled, format, path "
            "FROM storage_config WHERE user_id = ?");
        stmt.bind(1, user_id);
        std::vector<StorageConfig> results;
        while (stmt.executeStep()) {
            ColumnReader col(stmt);
            results.push_back({.user_id = col.nextI64(),
                               .type = col.nextEnum<StorageType>(),
                               .enabled = col.nextBool(),
                               .format = col.nextEnum<StorageFormat>(),
                               .path = col.nextText()});
        }
        return DbResult<std::vector<StorageConfig>>::ok(std::move(results));
    });
}

auto SqliteStorageConfigRepository::save(const StorageConfig& data) -> DbResult<StorageConfig> {
    return dbExecute<StorageConfig>("StorageConfigRepository::save", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO storage_config (user_id, type, enabled, format, path) "
            "VALUES (?, ?, ?, ?, ?)");
        ParamBinder(stmt)
            .i64(data.user_id)
            .asEnum(data.type)
            .boolean(data.enabled)
            .asEnum(data.format)
            .text(data.path);
        stmt.exec();
        return DbResult<StorageConfig>::ok(data);
    });
}

}  // namespace sst::db
