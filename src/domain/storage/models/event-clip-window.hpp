#pragma once

#include <cstdint>

namespace sst::storage {

// User-tunable pre/post-event window in seconds. Sourced from camera_config
// at trigger time so live edits take effect on the next clip.
struct EventClipWindow {
    int32_t pre_seconds{0};
    int32_t post_seconds{0};
};

}  // namespace sst::storage
