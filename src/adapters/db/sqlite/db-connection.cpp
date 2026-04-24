#include "adapters/db/sqlite/db-connection.hpp"

#include <spdlog/spdlog.h>

namespace sst::db {

DbConnection::DbConnection(const std::string& path)
    : db_(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
    db_.exec("PRAGMA journal_mode=WAL");
    db_.exec("PRAGMA foreign_keys=ON");
    spdlog::debug("SQLite opened: {}", path);
}

auto DbConnection::db() -> SQLite::Database& { return db_; }

}  // namespace sst::db
