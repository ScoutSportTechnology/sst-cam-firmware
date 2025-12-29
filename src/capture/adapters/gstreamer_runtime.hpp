#pragma once

#ifndef BUNDLED_GSTREAMER_RUNTIME
#define BUNDLED_GSTREAMER_RUNTIME 0
#endif

#include <glib.h>
#include <gst/gst.h>

#if BUNDLED_GSTREAMER_RUNTIME
#include <filesystem>
#endif

#include <mutex>

namespace sst::capture::adapters {

inline void GstInitRuntime() {
    static std::once_flag once;

    std::call_once(once, [] {
        gst_init(nullptr, nullptr);

#if BUNDLED_GSTREAMER_RUNTIME
        if (std::filesystem::exists("gstreamer-plugins")) {
            gst_registry_scan_path(gst_registry_get(), "gstreamer-plugins");
        }
#endif
    });
}

}  // namespace sst::capture::adapters
