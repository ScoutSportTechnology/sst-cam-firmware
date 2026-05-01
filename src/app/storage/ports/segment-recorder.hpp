#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "app/storage/ports/encoder-pipeline.hpp"

namespace sst::storage {

// Receives the full-game H.264 stream from the encoder and rotates segment
// mp4 files on pause/resume. Stop closes the current segment and merges all
// segments into one final mp4 via a software concat pipeline (no re-encode).
class ISegmentRecorder {
   public:
    virtual ~ISegmentRecorder() = default;

    // Begin a new full-game recording. `output_dir` must exist; segments land
    // at `<output_dir>/seg_NNNNN.mp4`. The merged file lands at
    // `<output_dir>/full_game.mp4` after Stop().
    virtual auto Start(const std::filesystem::path& output_dir) -> bool = 0;

    // Pause = close current segment, drop incoming buffers until Resume.
    virtual auto Pause() -> void = 0;
    virtual auto Resume() -> void = 0;

    // Stop = close current segment, run concat-merger over all segments,
    // return the merged file path. Empty string on failure.
    virtual auto Stop() -> std::filesystem::path = 0;

    [[nodiscard]] virtual auto IsRunning() const -> bool = 0;
    [[nodiscard]] virtual auto IsPaused() const -> bool = 0;

    // Produced segment paths in write order (cleared on each Start). Useful
    // for tests + DB persistence of `recording_segment` rows.
    [[nodiscard]] virtual auto Segments() const -> std::vector<std::filesystem::path> = 0;

    // Hook for the encoder pipeline tee.
    virtual auto OnEncoded(const EncodedNal& nal) -> void = 0;
};

}  // namespace sst::storage
