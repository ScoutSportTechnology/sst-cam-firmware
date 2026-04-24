#include "adapters/db/sqlite/user-repository.hpp"

namespace sst::db {

SqliteUserRepository::SqliteUserRepository(DbConnection& conn) : SqliteRepositoryBase(conn) {}

auto SqliteUserRepository::get(int64_t user_id) -> DbResult<User> {
    return dbExecute<User>("UserRepository::get", [&] {
        SQLite::Statement stmt(db_, "SELECT id, username FROM users WHERE id = ?");
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<User>::fail();
        }
        ColumnReader col(stmt);
        return DbResult<User>::ok({.id = col.nextI64(), .username = col.nextText()});
    });
}

auto SqliteUserRepository::create(const std::string& username) -> DbResult<User> {
    return dbExecute<User>("UserRepository::create", [&] {
        SQLite::Statement stmt(db_, "INSERT INTO users (username) VALUES (?)");
        stmt.bind(1, username);
        stmt.exec();
        return DbResult<User>::ok({.id = db_.getLastInsertRowid(), .username = username});
    });
}

}  // namespace sst::db
