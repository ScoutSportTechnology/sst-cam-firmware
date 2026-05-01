#include "adapters/db/sqlite/match-repository.hpp"

namespace sst::db {

SqliteMatchRepository::SqliteMatchRepository(DbConnection& conn) : SqliteRepositoryBase(conn) {}

namespace {

auto readMatch(SQLite::Statement& stmt) -> Match {
    ColumnReader col(stmt);
    return {.id = col.nextText(),
            .user_id = col.nextI64(),
            .sport_id = col.nextI64(),
            .team_a_id = col.nextI64(),
            .team_b_id = col.nextI64(),
            .name = col.nextOptText(),
            .started_at = col.nextText(),
            .ended_at = col.nextOptText(),
            .current_period = col.nextI32(),
            .final_score_a = col.nextI32(),
            .final_score_b = col.nextI32()};
}

auto readMatchEvent(SQLite::Statement& stmt) -> MatchEvent {
    ColumnReader col(stmt);
    return {.id = col.nextText(),
            .match_id = col.nextText(),
            .sport_id = col.nextI64(),
            .event_code = col.nextText(),
            .period = col.nextI32(),
            .timestamp_in_match = col.nextDouble(),
            .wall_clock_at = col.nextText(),
            .metadata_json = col.nextOptText()};
}

constexpr const char* kSelectMatchCols =
    "SELECT id, user_id, sport_id, team_a_id, team_b_id, name, started_at, ended_at, "
    "current_period, final_score_a, final_score_b FROM match";

constexpr const char* kSelectEventCols =
    "SELECT id, match_id, sport_id, event_code, period, timestamp_in_match, "
    "wall_clock_at, metadata_json FROM match_event";

}  // namespace

auto SqliteMatchRepository::save(const Match& data) -> DbResult<Match> {
    return dbExecute<Match>("MatchRepository::save", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO match "
            "(id, user_id, sport_id, team_a_id, team_b_id, name, started_at, ended_at, "
            " current_period, final_score_a, final_score_b) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        ParamBinder(stmt)
            .text(data.id)
            .i64(data.user_id)
            .i64(data.sport_id)
            .i64(data.team_a_id)
            .i64(data.team_b_id)
            .optText(data.name)
            .text(data.started_at)
            .optText(data.ended_at)
            .i32(data.current_period)
            .i32(data.final_score_a)
            .i32(data.final_score_b);
        stmt.exec();
        return DbResult<Match>::ok(data);
    });
}

auto SqliteMatchRepository::get(const std::string& match_id) -> DbResult<Match> {
    return dbExecute<Match>("MatchRepository::get", [&] {
        SQLite::Statement stmt(db_, std::string{kSelectMatchCols} + " WHERE id = ?");
        stmt.bind(1, match_id);
        if (!stmt.executeStep()) {
            return DbResult<Match>::fail();
        }
        return DbResult<Match>::ok(readMatch(stmt));
    });
}

auto SqliteMatchRepository::listByUser(int64_t user_id) -> DbResult<std::vector<Match>> {
    return dbExecute<std::vector<Match>>("MatchRepository::listByUser", [&] {
        SQLite::Statement stmt(
            db_, std::string{kSelectMatchCols} + " WHERE user_id = ? ORDER BY started_at DESC");
        stmt.bind(1, user_id);
        std::vector<Match> rows;
        while (stmt.executeStep()) {
            rows.push_back(readMatch(stmt));
        }
        return DbResult<std::vector<Match>>::ok(std::move(rows));
    });
}

auto SqliteMatchRepository::updatePeriod(const std::string& match_id, int32_t period)
    -> DbResult<bool> {
    return dbExecute<bool>("MatchRepository::updatePeriod", [&] {
        SQLite::Statement stmt(db_, "UPDATE match SET current_period = ? WHERE id = ?");
        ParamBinder(stmt).i32(period).text(match_id);
        stmt.exec();
        return DbResult<bool>::ok(db_.getChanges() > 0);
    });
}

auto SqliteMatchRepository::updateScore(const std::string& match_id, int32_t score_a,
                                        int32_t score_b) -> DbResult<bool> {
    return dbExecute<bool>("MatchRepository::updateScore", [&] {
        SQLite::Statement stmt(
            db_, "UPDATE match SET final_score_a = ?, final_score_b = ? WHERE id = ?");
        ParamBinder(stmt).i32(score_a).i32(score_b).text(match_id);
        stmt.exec();
        return DbResult<bool>::ok(db_.getChanges() > 0);
    });
}

auto SqliteMatchRepository::finalize(const std::string& match_id, const std::string& ended_at)
    -> DbResult<bool> {
    return dbExecute<bool>("MatchRepository::finalize", [&] {
        SQLite::Statement stmt(db_, "UPDATE match SET ended_at = ? WHERE id = ?");
        ParamBinder(stmt).text(ended_at).text(match_id);
        stmt.exec();
        return DbResult<bool>::ok(db_.getChanges() > 0);
    });
}

auto SqliteMatchRepository::remove(const std::string& match_id) -> DbResult<bool> {
    return dbExecute<bool>("MatchRepository::remove", [&] {
        SQLite::Statement stmt(db_, "DELETE FROM match WHERE id = ?");
        stmt.bind(1, match_id);
        stmt.exec();
        return DbResult<bool>::ok(db_.getChanges() > 0);
    });
}

auto SqliteMatchRepository::saveEvent(const MatchEvent& data) -> DbResult<MatchEvent> {
    return dbExecute<MatchEvent>("MatchRepository::saveEvent", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO match_event "
            "(id, match_id, sport_id, event_code, period, timestamp_in_match, "
            " wall_clock_at, metadata_json) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
        ParamBinder(stmt)
            .text(data.id)
            .text(data.match_id)
            .i64(data.sport_id)
            .text(data.event_code)
            .i32(data.period)
            .dbl(data.timestamp_in_match)
            .text(data.wall_clock_at)
            .optText(data.metadata_json);
        stmt.exec();
        return DbResult<MatchEvent>::ok(data);
    });
}

auto SqliteMatchRepository::getEvent(const std::string& event_id) -> DbResult<MatchEvent> {
    return dbExecute<MatchEvent>("MatchRepository::getEvent", [&] {
        SQLite::Statement stmt(db_, std::string{kSelectEventCols} + " WHERE id = ?");
        stmt.bind(1, event_id);
        if (!stmt.executeStep()) {
            return DbResult<MatchEvent>::fail();
        }
        return DbResult<MatchEvent>::ok(readMatchEvent(stmt));
    });
}

auto SqliteMatchRepository::listEvents(const std::string& match_id)
    -> DbResult<std::vector<MatchEvent>> {
    return dbExecute<std::vector<MatchEvent>>("MatchRepository::listEvents", [&] {
        SQLite::Statement stmt(
            db_, std::string{kSelectEventCols} +
                     " WHERE match_id = ? ORDER BY timestamp_in_match");
        stmt.bind(1, match_id);
        std::vector<MatchEvent> rows;
        while (stmt.executeStep()) {
            rows.push_back(readMatchEvent(stmt));
        }
        return DbResult<std::vector<MatchEvent>>::ok(std::move(rows));
    });
}

}  // namespace sst::db
