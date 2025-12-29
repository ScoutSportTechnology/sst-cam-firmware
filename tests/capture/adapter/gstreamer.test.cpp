#include <gst/gst.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include "capture/adapters/gstreamer_runtime.hpp"

TEST(GstreamerAdapter, SimplePipelineSmoke) {
    sst::capture::adapters::GstInitRuntime();

    const char* pipeline_str = "videotestsrc num-buffers=30 ! videoconvert ! fakesink sync=false";

    GError* err = nullptr;
    GstElement* pipeline = gst_parse_launch(pipeline_str, &err);

    if (pipeline == nullptr) {
        spdlog::error("gst_parse_launch FAILED for: {}", pipeline_str);
        spdlog::error("error: {}",
                      ((err != nullptr) && (err->message != nullptr)) ? err->message : "(null)");
        if (err != nullptr) {
            g_error_free(err);
        }
        FAIL();
    }
    if (err != nullptr) {
        g_error_free(err);
    }

    GstBus* bus = gst_element_get_bus(pipeline);

    ASSERT_NE(gst_element_set_state(pipeline, GST_STATE_PLAYING), GST_STATE_CHANGE_FAILURE);

    GstMessage* msg = gst_bus_timed_pop_filtered(
        bus, 2 * GST_SECOND, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if ((msg != nullptr) && GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError* gstreamerError = nullptr;
        gchar* dbg = nullptr;
        gst_message_parse_error(msg, &gstreamerError, &dbg);

        spdlog::error("BUS ERROR: {}",
                      ((gstreamerError != nullptr) && (gstreamerError->message != nullptr))
                          ? gstreamerError->message
                          : "(null)");
        if (dbg != nullptr) {
            spdlog::error("debug: {}", dbg);
        }

        if (gstreamerError != nullptr) {
            g_error_free(gstreamerError);
        }
        if (dbg != nullptr) {
            g_free(dbg);
        }

        gst_message_unref(msg);
        FAIL();
    }

    if (msg != nullptr) {
        gst_message_unref(msg);
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(pipeline);
}
