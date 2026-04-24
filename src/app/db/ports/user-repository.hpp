#pragma once

#include <string>

#include "domain/db/models/db-result.hpp"
#include "domain/db/models/user.hpp"

namespace sst::db {

class IUserRepository {
   public:
    virtual ~IUserRepository() = default;
    virtual auto get(int64_t user_id) -> DbResult<User> = 0;
    virtual auto create(const std::string& username) -> DbResult<User> = 0;
};

}  // namespace sst::db
