#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "domain/capture/models/frame.hpp"

namespace sst::raw_capture {

// Records raw frames from BOTH cameras to per-camera files, independently of the
// final (cam-0) recording. Owned by the record/stream lifecycle: Start opens the
// per-camera files for one match-derived capture_group_id, the pipeline producers
// push each camera's materialized frames in, Stop flushes and closes them.
//
// Implementations MUST NOT do synchronous file I/O on the calling (capture)
// thread — PushCamera hands off to a bounded leaky GStreamer queue so a slow
// disk can never stall capture.
class IRawCaptureSink {
   public:
    virtual ~IRawCaptureSink() = default;

    // Open per-camera files for a new proxy session stamped with the match-derived
    // capture_group_id, written INTO `output_dir` — the per-match directory
    // (the parent of SessionConfig.video_output_path), beside the final
    // <match>.mp4 + <match>.timeline.json rather than at the video root. An empty
    // output_dir falls back to the sink's construction-time root. Returns false
    // if already capturing or a file can't be opened. Independent of the final
    // recording — both run concurrently.
    virtual auto Start(const std::string& capture_group_id,
                       const std::filesystem::path& output_dir) -> bool = 0;

    // Enqueue a materialized frame from `camera_index`. Non-blocking: no-ops when
    // not capturing or the index is out of range; drops the oldest queued frame
    // rather than blocking the caller when the writer can't keep up.
    virtual auto PushCamera(std::uint32_t camera_index,
                            const sst::capture::Frame& frame) -> void = 0;

    // EOS each camera pipeline and close its MP4.
    // Returns false when no session was active. Idempotent enough for cleanup.
    virtual auto Stop() -> bool = 0;

    [[nodiscard]] virtual auto IsCapturing() const -> bool = 0;
};

}  // namespace sst::raw_capture
