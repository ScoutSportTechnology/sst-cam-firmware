#pragma once

#include "config/domain/profile_config.hpp"

namespace sst::config::domain {

inline auto apply_patch(ProfileData& modifiedData, const ProfileData& defaultData) -> void {
    if (defaultData.calibration) {
        modifiedData.calibration = defaultData.calibration;
    }
    if (defaultData.device) {
        modifiedData.device = defaultData.device;
    }
    if (defaultData.stream) {
        modifiedData.stream = defaultData.stream;
    }
    if (defaultData.storage) {
        modifiedData.storage = defaultData.storage;
    }
}

}  // namespace sst::config::domain
