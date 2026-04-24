#pragma once

#include "adapters/db/sqlite/db-helpers.hpp"
#include "app/db/ports/user-repository.hpp"

namespace sst::db {

class SqliteUserRepository : public IUserRepository, private SqliteRepositoryBase {
   public:
    explicit SqliteUserRepository(DbConnection& conn);
    auto get(int64_t user_id) -> DbResult<User> override;
    auto create(const std::string& username) -> DbResult<User> override;
};

}  // namespace sst::db
