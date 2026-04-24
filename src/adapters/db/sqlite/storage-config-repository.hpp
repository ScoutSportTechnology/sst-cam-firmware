#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <vector>

#include "adapters/db/sqlite/db-connection.hpp"
#include "app/db/ports/storage-config-repository.hpp"

namespace sst::db {

class SqliteStorageConfigRepository : public IStorageConfigRepository {
   public:
    explicit SqliteStorageConfigRepository(DbConnection& conn);
    auto getAll(int64_t user_id) -> DbResult<std::vector<StorageConfig>> override;
    auto save(const StorageConfig& data) -> DbResult<StorageConfig> override;

   private:
    SQLite::Database& db_;
};

}  // namespace sst::db
