#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include "adapters/db/sqlite/db-connection.hpp"
#include "app/db/ports/camera-repository.hpp"

namespace sst::db {

class SqliteCameraRepository : public ICameraRepository {
   public:
    explicit SqliteCameraRepository(DbConnection& conn);
    auto getConfig(int64_t user_id) -> DbResult<CameraConfig> override;
    auto saveConfig(const CameraConfig& data) -> DbResult<CameraConfig> override;
    auto getLatestCalibration(int32_t camera_id) -> DbResult<CameraCalibration> override;
    auto insertCalibration(const CameraCalibration& data) -> DbResult<CameraCalibration> override;

   private:
    SQLite::Database& db_;
};

}  // namespace sst::db
