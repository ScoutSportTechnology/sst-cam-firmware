#pragma once

#include <string>
#include <vector>

#include "domain/db/models/db-result.hpp"
#include "domain/db/models/match.hpp"

namespace sst::db {

class IMatchRepository {
   public:
    virtual ~IMatchRepository() = default;

    virtual auto save(const Match& data) -> DbResult<Match> = 0;
    virtual auto get(const std::string& match_id) -> DbResult<Match> = 0;
    virtual auto listByUser(int64_t user_id) -> DbResult<std::vector<Match>> = 0;
    virtual auto updatePeriod(const std::string& match_id, int32_t period) -> DbResult<bool> = 0;
    virtual auto updateScore(const std::string& match_id, int32_t score_a, int32_t score_b)
        -> DbResult<bool> = 0;
    virtual auto finalize(const std::string& match_id, const std::string& ended_at)
        -> DbResult<bool> = 0;
    virtual auto remove(const std::string& match_id) -> DbResult<bool> = 0;

    virtual auto saveEvent(const MatchEvent& data) -> DbResult<MatchEvent> = 0;
    virtual auto getEvent(const std::string& event_id) -> DbResult<MatchEvent> = 0;
    virtual auto listEvents(const std::string& match_id) -> DbResult<std::vector<MatchEvent>> = 0;
};

}  // namespace sst::db
