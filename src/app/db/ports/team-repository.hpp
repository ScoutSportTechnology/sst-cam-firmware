#pragma once

#include <vector>

#include "domain/db/models/db-result.hpp"
#include "domain/db/models/team.hpp"

namespace sst::db {

class ITeamRepository {
   public:
    virtual ~ITeamRepository() = default;

    virtual auto save(const Team& data) -> DbResult<Team> = 0;
    virtual auto get(int64_t team_id) -> DbResult<Team> = 0;
    virtual auto list(int64_t sport_id) -> DbResult<std::vector<Team>> = 0;
    virtual auto remove(int64_t team_id) -> DbResult<bool> = 0;

    virtual auto savePlayer(const Player& data) -> DbResult<Player> = 0;
    virtual auto getPlayer(int64_t player_id) -> DbResult<Player> = 0;
    virtual auto listPlayers(int64_t team_id) -> DbResult<std::vector<Player>> = 0;
    virtual auto removePlayer(int64_t player_id) -> DbResult<bool> = 0;
};

}  // namespace sst::db
