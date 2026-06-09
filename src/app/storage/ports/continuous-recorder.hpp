#pragma once

#include <filesystem>

#include "domain/capture/models/frame.hpp"

namespace sst::storage {

// One continuous, playable MP4 per session. Pause/Resume gate the muxer into the
// SAME file (drop-to-IDR on resume); Stop EOSes the muxer to a finalized,
// seekable file. Implemented by a GStreamer NVENC adapter (hardware-bound).
class IContinuousRecorder {
   public:
    virtual ~IContinuousRecorder() = default;

    virtual auto Start(const std::filesystem::path& output_mp4) -> bool = 0;
    virtual auto Pause() -> void = 0;
    virtual auto Resume() -> void = 0;
    // EOS the muxer and finalize a playable file. Returns false if not running.
    virtual auto Stop() -> bool = 0;
    [[nodiscard]] virtual auto IsRunning() const -> bool = 0;

    virtual auto Push(const sst::capture::Frame& frame) -> void = 0;
};

}  // namespace sst::storage
