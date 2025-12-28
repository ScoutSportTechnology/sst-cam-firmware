#pragma once

#include <glib.h>
#include <gst/gst.h>

#include <filesystem>
#include <mutex>

namespace sst::capture::adapters {

inline void GstInitWithLocalPluginsOnce() {
    static std::once_flag once;

    std::call_once(once, [] {
        gst_init(nullptr, nullptr);

        const gchar* plugin_path = g_getenv("GST_PLUGIN_PATH_1_0");
        if (plugin_path && *plugin_path) {
            return;
        }

        if (std::filesystem::exists("gstreamer-plugins")) {
            gst_registry_scan_path(gst_registry_get(), "gstreamer-plugins");
        }
    });
}

}  // namespace sst::capture::adapters
