#pragma once

#include <string>
#include <vector>

#include "domain/db/models/db-result.hpp"
#include "domain/db/models/sport.hpp"

namespace sst::db {

class ISportRepository {
   public:
    virtual ~ISportRepository() = default;

    virtual auto save(const Sport& data) -> DbResult<Sport> = 0;
    virtual auto get(int64_t id) -> DbResult<Sport> = 0;
    virtual auto getByCode(const std::string& code) -> DbResult<Sport> = 0;
    virtual auto list() -> DbResult<std::vector<Sport>> = 0;
    virtual auto remove(int64_t id) -> DbResult<bool> = 0;

    virtual auto saveEventKind(const SportEventKind& data) -> DbResult<SportEventKind> = 0;
    virtual auto getEventKind(int64_t sport_id, const std::string& code)
        -> DbResult<SportEventKind> = 0;
    virtual auto listEventKinds(int64_t sport_id) -> DbResult<std::vector<SportEventKind>> = 0;
    virtual auto removeEventKind(int64_t sport_id, const std::string& code) -> DbResult<bool> = 0;
};

}  // namespace sst::db
