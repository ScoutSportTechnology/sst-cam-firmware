#pragma once

#ifndef BUNDLED_GSTREAMER_RUNTIME
#define BUNDLED_GSTREAMER_RUNTIME 0
#endif

#include <glib.h>
#include <gst/gst.h>
#include <spdlog/spdlog.h>

#include <mutex>

namespace sst::capture::adapters {

inline void GstInitRuntime(int* argc = nullptr, char*** argv = nullptr) {
    static std::once_flag once;

    std::call_once(once, [&] {
        gst_init(argc, argv);

#if BUNDLED_GSTREAMER_RUNTIME
        if (std::filesystem::exists("gstreamer-plugins")) {
            gst_registry_scan_path(gst_registry_get(), "gstreamer-plugins");
            spdlog::info("GStreamer: scanned bundled plugins");
        }
#else
        spdlog::info("GStreamer: using system-installed runtime");

#endif
    });
}

}  // namespace sst::capture::adapters
