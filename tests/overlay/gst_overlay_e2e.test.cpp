// GStreamer overlay compositor (U9, R20). Hardware-bound: needs working
// GStreamer plugins (appsrc/compositor) — expected to FAIL in the cross-compile
// container, passes on-device.

#include "adapters/overlay/gstreamer/gst-overlay-compositor.hpp"

#include <gtest/gtest.h>

#include "domain/overlay/models/render-scene.hpp"

namespace {

TEST(GstOverlayE2E, AppSrcAcceptsRgbaPush) {
    sst::adapters::overlay::GstOverlayCompositor compositor(320, 180);
    ASSERT_NE(compositor.AppSrc(), nullptr);

    sst::overlay::RgbaImage frame;
    frame.width = 320;
    frame.height = 180;
    frame.stride = 320 * 4;
    frame.pixels.assign(static_cast<std::size_t>(frame.stride) * 180, 0x80);

    // On-device this pushes a buffer into the live appsrc without error.
    compositor.PushFrame(frame);
    SUCCEED();
}

}  // namespace
