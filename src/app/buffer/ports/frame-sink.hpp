#pragma once

#include "domain/capture/models/frame.hpp"

namespace sst::buffer {

// Single ingestion point for downstream consumers of the post-processed
// final-frame stream. Storage and streaming each implement this and the
// pipeline orchestration thread (separate task) fans out one Push per
// final frame to every active sink.
class IFrameSink {
   public:
    virtual ~IFrameSink() = default;

    virtual auto Push(const sst::capture::Frame& frame) -> void = 0;
};

}  // namespace sst::buffer
