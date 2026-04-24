#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <vector>

#include "adapters/db/sqlite/db-connection.hpp"
#include "app/db/ports/stream-config-repository.hpp"

namespace sst::db {

class SqliteStreamConfigRepository : public IStreamConfigRepository {
   public:
    explicit SqliteStreamConfigRepository(DbConnection& conn);
    auto getAll(int64_t user_id) -> DbResult<std::vector<StreamConfig>> override;
    auto save(const StreamConfig& data) -> DbResult<StreamConfig> override;
    auto remove(int64_t stream_id) -> bool override;

   private:
    SQLite::Database& db_;
};

}  // namespace sst::db
