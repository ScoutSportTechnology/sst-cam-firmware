#pragma once

#include <vector>

#include "adapters/db/sqlite/db-helpers.hpp"
#include "app/db/ports/storage-config-repository.hpp"

namespace sst::db {

class SqliteStorageConfigRepository : public IStorageConfigRepository,
                                      private SqliteRepositoryBase {
   public:
    explicit SqliteStorageConfigRepository(DbConnection& conn);
    auto getAll(int64_t user_id) -> DbResult<std::vector<StorageConfig>> override;
    auto save(const StorageConfig& data) -> DbResult<StorageConfig> override;
};

}  // namespace sst::db
