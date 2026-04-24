#include "adapters/db/sqlite/user-repository.hpp"

#include <spdlog/spdlog.h>

namespace sst::db {

SqliteUserRepository::SqliteUserRepository(DbConnection& conn) : db_(conn.db()) {}

auto SqliteUserRepository::get(int64_t user_id) -> DbResult<User> {
    try {
        SQLite::Statement stmt(db_, "SELECT id, username FROM users WHERE id = ?");
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<User>::fail();
        }
        return DbResult<User>::ok(
            {.id = stmt.getColumn(0).getInt64(), .username = stmt.getColumn(1).getText()});
    } catch (const SQLite::Exception& ex) {
        spdlog::error("UserRepository::get failed: {}", ex.what());
        return DbResult<User>::fail();
    }
}

auto SqliteUserRepository::create(const std::string& username) -> DbResult<User> {
    try {
        SQLite::Statement stmt(db_, "INSERT INTO users (username) VALUES (?)");
        stmt.bind(1, username);
        stmt.exec();
        return DbResult<User>::ok({.id = db_.getLastInsertRowid(), .username = username});
    } catch (const SQLite::Exception& ex) {
        spdlog::error("UserRepository::create failed: {}", ex.what());
        return DbResult<User>::fail();
    }
}

}  // namespace sst::db
