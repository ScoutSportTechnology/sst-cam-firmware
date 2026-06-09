#pragma once

#include <optional>

#include "domain/capture/models/frame.hpp"

namespace sst::pipeline {

// Source of the most recent fully-processed final frame, owned in memory. The
// on-demand BLE ThumbnailRequest path grabs a snapshot here and encodes it to
// JPEG bytes (no disk round-trip). GrabLatest() returns std::nullopt when no
// frame has been produced yet (camera idle / pipeline not started) — the caller
// surfaces that as a ResponseStatus::ERROR rather than an empty success.
//
// The returned Frame owns its pixels (deep-copied off any GstBuffer) so it stays
// valid after the call returns, independent of the producer overwriting the
// latest slot.
class IFrameSnapshotSource {
   public:
    virtual ~IFrameSnapshotSource() = default;

    [[nodiscard]] virtual auto GrabLatest() -> std::optional<sst::capture::Frame> = 0;
};

}  // namespace sst::pipeline
