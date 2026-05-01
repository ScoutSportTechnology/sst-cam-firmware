#pragma once

#include <cstdint>

namespace sst::storage {

enum class RecordingState : uint8_t {
    kIdle = 0,       // No full-game recording in progress.
    kRecording = 1,  // Encoder + segment recorder both running.
    kPaused = 2,     // Encoder running (so the event-clip ring stays warm), segment recorder paused.
};

}  // namespace sst::storage
