#pragma once

#include <vector>

#include "adapters/db/sqlite/db-helpers.hpp"
#include "app/db/ports/stream-config-repository.hpp"

namespace sst::db {

class SqliteStreamConfigRepository : public IStreamConfigRepository,
                                     private SqliteRepositoryBase {
   public:
    explicit SqliteStreamConfigRepository(DbConnection& conn);
    auto getAll(int64_t user_id) -> DbResult<std::vector<StreamConfig>> override;
    auto save(const StreamConfig& data) -> DbResult<StreamConfig> override;
    auto remove(int64_t stream_id) -> DbResult<bool> override;
};

}  // namespace sst::db
