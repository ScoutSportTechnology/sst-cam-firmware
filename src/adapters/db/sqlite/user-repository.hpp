#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include "adapters/db/sqlite/db-connection.hpp"
#include "app/db/ports/user-repository.hpp"

namespace sst::db {

class SqliteUserRepository : public IUserRepository {
   public:
    explicit SqliteUserRepository(DbConnection& conn);
    auto get(int64_t user_id) -> DbResult<User> override;
    auto create(const std::string& username) -> DbResult<User> override;

   private:
    SQLite::Database& db_;
};

}  // namespace sst::db
