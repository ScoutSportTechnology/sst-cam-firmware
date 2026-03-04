#pragma once

#include "domain/config/models/profile.hpp"

namespace sst::config {

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

}  // namespace sst::config
