#pragma once

#include <vector>

#include "domain/db/models/db-result.hpp"
#include "domain/db/models/stream-config.hpp"

namespace sst::db {

class IStreamConfigRepository {
   public:
    virtual ~IStreamConfigRepository() = default;
    virtual auto getAll(int64_t user_id) -> DbResult<std::vector<StreamConfig>> = 0;
    virtual auto save(const StreamConfig& data) -> DbResult<StreamConfig> = 0;
    virtual auto remove(int64_t stream_id) -> DbResult<bool> = 0;
};

}  // namespace sst::db
