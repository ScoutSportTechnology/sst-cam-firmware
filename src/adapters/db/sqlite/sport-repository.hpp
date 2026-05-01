#pragma once

#include <string>
#include <vector>

#include "adapters/db/sqlite/db-helpers.hpp"
#include "app/db/ports/sport-repository.hpp"

namespace sst::db {

class SqliteSportRepository : public ISportRepository, private SqliteRepositoryBase {
   public:
    explicit SqliteSportRepository(DbConnection& conn);

    auto save(const Sport& data) -> DbResult<Sport> override;
    auto get(int64_t id) -> DbResult<Sport> override;
    auto getByCode(const std::string& code) -> DbResult<Sport> override;
    auto list() -> DbResult<std::vector<Sport>> override;
    auto remove(int64_t id) -> DbResult<bool> override;

    auto saveEventKind(const SportEventKind& data) -> DbResult<SportEventKind> override;
    auto getEventKind(int64_t sport_id, const std::string& code)
        -> DbResult<SportEventKind> override;
    auto listEventKinds(int64_t sport_id) -> DbResult<std::vector<SportEventKind>> override;
    auto removeEventKind(int64_t sport_id, const std::string& code) -> DbResult<bool> override;
};

}  // namespace sst::db
