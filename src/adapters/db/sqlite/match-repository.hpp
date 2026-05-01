#pragma once

#include <string>
#include <vector>

#include "adapters/db/sqlite/db-helpers.hpp"
#include "app/db/ports/match-repository.hpp"

namespace sst::db {

class SqliteMatchRepository : public IMatchRepository, private SqliteRepositoryBase {
   public:
    explicit SqliteMatchRepository(DbConnection& conn);

    auto save(const Match& data) -> DbResult<Match> override;
    auto get(const std::string& match_id) -> DbResult<Match> override;
    auto listByUser(int64_t user_id) -> DbResult<std::vector<Match>> override;
    auto updatePeriod(const std::string& match_id, int32_t period) -> DbResult<bool> override;
    auto updateScore(const std::string& match_id, int32_t score_a, int32_t score_b)
        -> DbResult<bool> override;
    auto finalize(const std::string& match_id, const std::string& ended_at)
        -> DbResult<bool> override;
    auto remove(const std::string& match_id) -> DbResult<bool> override;

    auto saveEvent(const MatchEvent& data) -> DbResult<MatchEvent> override;
    auto getEvent(const std::string& event_id) -> DbResult<MatchEvent> override;
    auto listEvents(const std::string& match_id) -> DbResult<std::vector<MatchEvent>> override;
};

}  // namespace sst::db
