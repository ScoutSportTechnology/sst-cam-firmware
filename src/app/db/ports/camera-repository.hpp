#pragma once

#include "domain/db/models/camera.hpp"
#include "domain/db/models/db-result.hpp"

namespace sst::db {

class ICameraRepository {
   public:
    virtual ~ICameraRepository() = default;
    virtual auto getConfig(int64_t user_id) -> DbResult<CameraConfig> = 0;
    virtual auto saveConfig(const CameraConfig& data) -> DbResult<CameraConfig> = 0;
    virtual auto getLatestCalibration(int32_t camera_id) -> DbResult<CameraCalibration> = 0;
    virtual auto insertCalibration(const CameraCalibration& data) -> DbResult<CameraCalibration> = 0;
};

}  // namespace sst::db
