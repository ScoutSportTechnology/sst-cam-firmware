#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include "adapters/db/sqlite/db-connection.hpp"
#include "app/db/ports/microphone-repository.hpp"

namespace sst::db {

class SqliteMicrophoneRepository : public IMicrophoneRepository {
   public:
    explicit SqliteMicrophoneRepository(DbConnection& conn);
    auto getConfig(int64_t user_id) -> DbResult<MicrophoneConfig> override;
    auto saveConfig(const MicrophoneConfig& data) -> DbResult<MicrophoneConfig> override;
    auto getLatestCalibration(int32_t mic_id) -> DbResult<MicrophoneCalibration> override;
    auto insertCalibration(const MicrophoneCalibration& data)
        -> DbResult<MicrophoneCalibration> override;

   private:
    SQLite::Database& db_;
};

}  // namespace sst::db
