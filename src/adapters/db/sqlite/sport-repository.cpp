#include "adapters/db/sqlite/sport-repository.hpp"

namespace sst::db {

SqliteSportRepository::SqliteSportRepository(DbConnection& conn) : SqliteRepositoryBase(conn) {}

namespace {

auto readSport(SQLite::Statement& stmt) -> Sport {
    ColumnReader col(stmt);
    return {.id = col.nextI64(),
            .code = col.nextText(),
            .name = col.nextText(),
            .periods = col.nextI32()};
}

auto readEventKind(SQLite::Statement& stmt) -> SportEventKind {
    ColumnReader col(stmt);
    return {.sport_id = col.nextI64(), .code = col.nextText(), .name = col.nextText()};
}

}  // namespace

auto SqliteSportRepository::save(const Sport& data) -> DbResult<Sport> {
    return dbExecute<Sport>("SportRepository::save", [&] {
        Sport saved = data;
        if (data.id == 0) {
            SQLite::Statement stmt(db_,
                                   "INSERT INTO sport (code, name, periods) VALUES (?, ?, ?)");
            ParamBinder(stmt).text(data.code).text(data.name).i32(data.periods);
            stmt.exec();
            saved.id = db_.getLastInsertRowid();
        } else {
            SQLite::Statement stmt(
                db_,
                "INSERT OR REPLACE INTO sport (id, code, name, periods) VALUES (?, ?, ?, ?)");
            ParamBinder(stmt).i64(data.id).text(data.code).text(data.name).i32(data.periods);
            stmt.exec();
        }
        return DbResult<Sport>::ok(std::move(saved));
    });
}

auto SqliteSportRepository::get(int64_t id) -> DbResult<Sport> {
    return dbExecute<Sport>("SportRepository::get", [&] {
        SQLite::Statement stmt(db_, "SELECT id, code, name, periods FROM sport WHERE id = ?");
        stmt.bind(1, id);
        if (!stmt.executeStep()) {
            return DbResult<Sport>::fail();
        }
        return DbResult<Sport>::ok(readSport(stmt));
    });
}

auto SqliteSportRepository::getByCode(const std::string& code) -> DbResult<Sport> {
    return dbExecute<Sport>("SportRepository::getByCode", [&] {
        SQLite::Statement stmt(db_, "SELECT id, code, name, periods FROM sport WHERE code = ?");
        stmt.bind(1, code);
        if (!stmt.executeStep()) {
            return DbResult<Sport>::fail();
        }
        return DbResult<Sport>::ok(readSport(stmt));
    });
}

auto SqliteSportRepository::list() -> DbResult<std::vector<Sport>> {
    return dbExecute<std::vector<Sport>>("SportRepository::list", [&] {
        SQLite::Statement stmt(db_, "SELECT id, code, name, periods FROM sport ORDER BY code");
        std::vector<Sport> rows;
        while (stmt.executeStep()) {
            rows.push_back(readSport(stmt));
        }
        return DbResult<std::vector<Sport>>::ok(std::move(rows));
    });
}

auto SqliteSportRepository::remove(int64_t id) -> DbResult<bool> {
    return dbExecute<bool>("SportRepository::remove", [&] {
        SQLite::Statement stmt(db_, "DELETE FROM sport WHERE id = ?");
        stmt.bind(1, id);
        stmt.exec();
        return DbResult<bool>::ok(db_.getChanges() > 0);
    });
}

auto SqliteSportRepository::saveEventKind(const SportEventKind& data)
    -> DbResult<SportEventKind> {
    return dbExecute<SportEventKind>("SportRepository::saveEventKind", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO sport_event_kind (sport_id, code, name) VALUES (?, ?, ?)");
        ParamBinder(stmt).i64(data.sport_id).text(data.code).text(data.name);
        stmt.exec();
        return DbResult<SportEventKind>::ok(data);
    });
}

auto SqliteSportRepository::getEventKind(int64_t sport_id, const std::string& code)
    -> DbResult<SportEventKind> {
    return dbExecute<SportEventKind>("SportRepository::getEventKind", [&] {
        SQLite::Statement stmt(db_,
                               "SELECT sport_id, code, name FROM sport_event_kind "
                               "WHERE sport_id = ? AND code = ?");
        stmt.bind(1, sport_id);
        stmt.bind(2, code);
        if (!stmt.executeStep()) {
            return DbResult<SportEventKind>::fail();
        }
        return DbResult<SportEventKind>::ok(readEventKind(stmt));
    });
}

auto SqliteSportRepository::listEventKinds(int64_t sport_id)
    -> DbResult<std::vector<SportEventKind>> {
    return dbExecute<std::vector<SportEventKind>>("SportRepository::listEventKinds", [&] {
        SQLite::Statement stmt(db_,
                               "SELECT sport_id, code, name FROM sport_event_kind "
                               "WHERE sport_id = ? ORDER BY code");
        stmt.bind(1, sport_id);
        std::vector<SportEventKind> rows;
        while (stmt.executeStep()) {
            rows.push_back(readEventKind(stmt));
        }
        return DbResult<std::vector<SportEventKind>>::ok(std::move(rows));
    });
}

auto SqliteSportRepository::removeEventKind(int64_t sport_id, const std::string& code)
    -> DbResult<bool> {
    return dbExecute<bool>("SportRepository::removeEventKind", [&] {
        SQLite::Statement stmt(db_,
                               "DELETE FROM sport_event_kind WHERE sport_id = ? AND code = ?");
        stmt.bind(1, sport_id);
        stmt.bind(2, code);
        stmt.exec();
        return DbResult<bool>::ok(db_.getChanges() > 0);
    });
}

}  // namespace sst::db
