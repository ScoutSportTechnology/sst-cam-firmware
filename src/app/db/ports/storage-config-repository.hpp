#pragma once

#include <vector>

#include "domain/db/models/db-result.hpp"
#include "domain/db/models/storage-config.hpp"

namespace sst::db {

class IStorageConfigRepository {
   public:
    virtual ~IStorageConfigRepository() = default;
    virtual auto getAll(int64_t user_id) -> DbResult<std::vector<StorageConfig>> = 0;
    virtual auto save(const StorageConfig& data) -> DbResult<StorageConfig> = 0;
};

}  // namespace sst::db
