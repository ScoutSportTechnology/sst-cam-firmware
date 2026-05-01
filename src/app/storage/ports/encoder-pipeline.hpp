#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "domain/capture/models/frame.hpp"

namespace sst::storage {

// One encoded H.264 NAL (or fragment) emitted by the encoder pipeline,
// timestamped against the encoder's running clock. The "is_keyframe" flag is
// what the event-clip ring uses to decide where to splice — clips must start
// on an IDR.
struct EncodedNal {
    std::vector<uint8_t> bytes;
    uint64_t pts_ns{0};
    uint64_t dts_ns{0};
    uint64_t duration_ns{0};
    bool is_keyframe{false};
};

// Single shared NVENC session that consumes raw frames and fans out the encoded
// H.264 stream to two consumers via tee: the segment recorder (full-game mp4
// rotation) and the event-clip ring (rolling pre-event window).
class IEncoderPipeline {
   public:
    using EncodedSink = std::function<void(const EncodedNal&)>;

    virtual ~IEncoderPipeline() = default;

    // Wire downstream consumers BEFORE Start(). Each callback receives every
    // encoded buffer (the tee duplicates the stream).
    virtual auto SetSegmentSink(EncodedSink sink) -> void = 0;
    virtual auto SetRingSink(EncodedSink sink) -> void = 0;

    virtual auto Start() -> bool = 0;
    virtual auto Stop() -> void = 0;
    [[nodiscard]] virtual auto IsRunning() const -> bool = 0;

    virtual auto Push(const sst::capture::Frame& frame) -> void = 0;
};

}  // namespace sst::storage
