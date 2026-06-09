#pragma once

#include "domain/capture/models/frame.hpp"

namespace sst::buffer {

// Deep-copies all of `src`'s plane bytes into a single owned std::vector<uint8_t>
// and returns a new Frame whose `owner` shared_ptr keeps that vector alive.
//
// Strides, geometry, format, frame_id, memory, and captured_at are preserved
// verbatim. The original `src.owner` refcount is not touched.
//
// Intended use: producers feeding a Frame (or a struct containing a Frame, e.g.
// FrameBundle) into a downstream buffer call this first, so the underlying
// GStreamer buffer is released immediately and does not pin an appsink slot
// while the Frame waits in the buffer.
auto MaterializeFrame(const sst::capture::Frame& src) -> sst::capture::Frame;

}  // namespace sst::buffer
