#pragma once

#include <filesystem>

#include "app/storage/ports/encoder-pipeline.hpp"
#include "domain/storage/models/event-clip-window.hpp"

namespace sst::storage {

// Maintains a rolling pre-event ring of encoded H.264 NALs and, on Trigger,
// snapshots back to the nearest IDR + records `post_seconds` more of live
// encoded output into a fresh mp4 file. Independent file, independent muxer
// — does not touch the segment recorder.
class IEventClipRecorder {
   public:
    virtual ~IEventClipRecorder() = default;

    // Hook for the encoder pipeline tee. Frees / sizes the ring as
    // `max_pre_seconds * keyframes_per_second + GOP_safety` so live edits to
    // `event_clip_pre_seconds` take effect on the next trigger.
    virtual auto OnEncoded(const EncodedNal& nal) -> void = 0;

    // Cut a clip starting `window.pre_seconds` back from now and ending
    // `window.post_seconds` after now, written to `output_path`. Returns true
    // when the post-event recorder accepted the request; the file is written
    // asynchronously and is complete once IsBusy() returns false.
    virtual auto Trigger(const std::filesystem::path& output_path,
                         const EventClipWindow& window) -> bool = 0;

    // True from the moment Trigger is accepted until the post-event mp4 is
    // closed. Useful for tests + for the BLE caller that wants a synchronous
    // confirmation before responding.
    [[nodiscard]] virtual auto IsBusy() const -> bool = 0;
};

}  // namespace sst::storage
