#pragma once

#include "domain/capture/models/frame.hpp"

namespace sst::processing {

// Output of preprocess. Both frames carry the same frame_id and captured_at.
//
// `source_frame`: a value-copy of the raw input frame. It shares the input's
// `owner` shared_ptr, so on creation it still pins whatever pinned the raw
// frame (e.g. a GstBuffer). Producers are expected to call
// sst::buffer::MaterializeFrame on this field before pushing the bundle into
// any downstream buffer, swapping the GstBuffer-backed owner for an owned
// std::vector<uint8_t>.
//
// `ai_frame`: a freshly built frame at PreprocessConfig.{ai_width, ai_height}
// in a format determined by PreprocessConfig.ai_color_mode (GRAY8 for
// Grayscale or Binary; RGB8 for RGB). Owns its own buffer; no GStreamer
// dependency.
struct FrameBundle {
    sst::capture::Frame source_frame;
    sst::capture::Frame ai_frame;
};

}  // namespace sst::processing
