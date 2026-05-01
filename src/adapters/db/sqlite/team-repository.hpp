#pragma once

#include <vector>

#include "adapters/db/sqlite/db-helpers.hpp"
#include "app/db/ports/team-repository.hpp"

namespace sst::db {

class SqliteTeamRepository : public ITeamRepository, private SqliteRepositoryBase {
   public:
    explicit SqliteTeamRepository(DbConnection& conn);

    auto save(const Team& data) -> DbResult<Team> override;
    auto get(int64_t team_id) -> DbResult<Team> override;
    auto list(int64_t sport_id) -> DbResult<std::vector<Team>> override;
    auto remove(int64_t team_id) -> DbResult<bool> override;

    auto savePlayer(const Player& data) -> DbResult<Player> override;
    auto getPlayer(int64_t player_id) -> DbResult<Player> override;
    auto listPlayers(int64_t team_id) -> DbResult<std::vector<Player>> override;
    auto removePlayer(int64_t player_id) -> DbResult<bool> override;
};

}  // namespace sst::db
