#pragma once

#include <cstdint>

namespace sst::storage {

enum class RecordingState : uint8_t {
    kIdle = 0,       // No recording in progress.
    kRecording = 1,  // Continuous MP4 muxer running, frames being written.
    kPaused = 2,     // Recording paused; muxer not fed (resumes into the SAME file).
};

}  // namespace sst::storage
