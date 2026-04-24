#pragma once

#include "domain/db/models/db-result.hpp"
#include "domain/db/models/microphone.hpp"

namespace sst::db {

class IMicrophoneRepository {
   public:
    virtual ~IMicrophoneRepository() = default;
    virtual auto getConfig(int64_t user_id) -> DbResult<MicrophoneConfig> = 0;
    virtual auto saveConfig(const MicrophoneConfig& data) -> DbResult<MicrophoneConfig> = 0;
    virtual auto getLatestCalibration(int32_t mic_id) -> DbResult<MicrophoneCalibration> = 0;
    virtual auto insertCalibration(const MicrophoneCalibration& data) -> DbResult<MicrophoneCalibration> = 0;
};

}  // namespace sst::db
