#include "adapters/db/sqlite/team-repository.hpp"

namespace sst::db {

SqliteTeamRepository::SqliteTeamRepository(DbConnection& conn) : SqliteRepositoryBase(conn) {}

namespace {

auto readTeam(SQLite::Statement& stmt) -> Team {
    ColumnReader col(stmt);
    return {.id = col.nextI64(),
            .sport_id = col.nextI64(),
            .name = col.nextText(),
            .short_name = col.nextText()};
}

auto readPlayer(SQLite::Statement& stmt) -> Player {
    ColumnReader col(stmt);
    return {.id = col.nextI64(),
            .team_id = col.nextI64(),
            .name = col.nextText(),
            .jersey_number = col.nextOptI32()};
}

}  // namespace

auto SqliteTeamRepository::save(const Team& data) -> DbResult<Team> {
    return dbExecute<Team>("TeamRepository::save", [&] {
        Team saved = data;
        if (data.id == 0) {
            SQLite::Statement stmt(
                db_, "INSERT INTO team (sport_id, name, short_name) VALUES (?, ?, ?)");
            ParamBinder(stmt).i64(data.sport_id).text(data.name).text(data.short_name);
            stmt.exec();
            saved.id = db_.getLastInsertRowid();
        } else {
            SQLite::Statement stmt(db_,
                                   "INSERT OR REPLACE INTO team (id, sport_id, name, short_name) "
                                   "VALUES (?, ?, ?, ?)");
            ParamBinder(stmt)
                .i64(data.id)
                .i64(data.sport_id)
                .text(data.name)
                .text(data.short_name);
            stmt.exec();
        }
        return DbResult<Team>::ok(std::move(saved));
    });
}

auto SqliteTeamRepository::get(int64_t team_id) -> DbResult<Team> {
    return dbExecute<Team>("TeamRepository::get", [&] {
        SQLite::Statement stmt(db_,
                               "SELECT id, sport_id, name, short_name FROM team WHERE id = ?");
        stmt.bind(1, team_id);
        if (!stmt.executeStep()) {
            return DbResult<Team>::fail();
        }
        return DbResult<Team>::ok(readTeam(stmt));
    });
}

auto SqliteTeamRepository::list(int64_t sport_id) -> DbResult<std::vector<Team>> {
    return dbExecute<std::vector<Team>>("TeamRepository::list", [&] {
        SQLite::Statement stmt(
            db_, "SELECT id, sport_id, name, short_name FROM team WHERE sport_id = ? ORDER BY name");
        stmt.bind(1, sport_id);
        std::vector<Team> rows;
        while (stmt.executeStep()) {
            rows.push_back(readTeam(stmt));
        }
        return DbResult<std::vector<Team>>::ok(std::move(rows));
    });
}

auto SqliteTeamRepository::remove(int64_t team_id) -> DbResult<bool> {
    return dbExecute<bool>("TeamRepository::remove", [&] {
        SQLite::Statement stmt(db_, "DELETE FROM team WHERE id = ?");
        stmt.bind(1, team_id);
        stmt.exec();
        return DbResult<bool>::ok(db_.getChanges() > 0);
    });
}

auto SqliteTeamRepository::savePlayer(const Player& data) -> DbResult<Player> {
    return dbExecute<Player>("TeamRepository::savePlayer", [&] {
        Player saved = data;
        if (data.id == 0) {
            SQLite::Statement stmt(
                db_, "INSERT INTO player (team_id, name, jersey_number) VALUES (?, ?, ?)");
            ParamBinder(stmt).i64(data.team_id).text(data.name).optI32(data.jersey_number);
            stmt.exec();
            saved.id = db_.getLastInsertRowid();
        } else {
            SQLite::Statement stmt(
                db_,
                "INSERT OR REPLACE INTO player (id, team_id, name, jersey_number) "
                "VALUES (?, ?, ?, ?)");
            ParamBinder(stmt)
                .i64(data.id)
                .i64(data.team_id)
                .text(data.name)
                .optI32(data.jersey_number);
            stmt.exec();
        }
        return DbResult<Player>::ok(std::move(saved));
    });
}

auto SqliteTeamRepository::getPlayer(int64_t player_id) -> DbResult<Player> {
    return dbExecute<Player>("TeamRepository::getPlayer", [&] {
        SQLite::Statement stmt(
            db_, "SELECT id, team_id, name, jersey_number FROM player WHERE id = ?");
        stmt.bind(1, player_id);
        if (!stmt.executeStep()) {
            return DbResult<Player>::fail();
        }
        return DbResult<Player>::ok(readPlayer(stmt));
    });
}

auto SqliteTeamRepository::listPlayers(int64_t team_id) -> DbResult<std::vector<Player>> {
    return dbExecute<std::vector<Player>>("TeamRepository::listPlayers", [&] {
        SQLite::Statement stmt(db_,
                               "SELECT id, team_id, name, jersey_number FROM player "
                               "WHERE team_id = ? ORDER BY jersey_number, name");
        stmt.bind(1, team_id);
        std::vector<Player> rows;
        while (stmt.executeStep()) {
            rows.push_back(readPlayer(stmt));
        }
        return DbResult<std::vector<Player>>::ok(std::move(rows));
    });
}

auto SqliteTeamRepository::removePlayer(int64_t player_id) -> DbResult<bool> {
    return dbExecute<bool>("TeamRepository::removePlayer", [&] {
        SQLite::Statement stmt(db_, "DELETE FROM player WHERE id = ?");
        stmt.bind(1, player_id);
        stmt.exec();
        return DbResult<bool>::ok(db_.getChanges() > 0);
    });
}

}  // namespace sst::db
