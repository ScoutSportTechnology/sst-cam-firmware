#pragma once

#include <cstdint>
#include <string>

namespace sst::db {

struct MicrophoneConfig {
    int64_t user_id{0};
    bool noise_reduction{true};
};

struct MicrophoneCalibration {
    int64_t id{0};
    int32_t mic_id{0};
    float sensitivity{1.0F};
    std::string calibrated_at;
};

}  // namespace sst::db
