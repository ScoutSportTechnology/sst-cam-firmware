#pragma once
#include <nlohmann/json.hpp>

#include "domain/config/models/storage.hpp"

namespace sst::config::domain {

using nlohmann::json;

inline void to_json(json& jsonObject, const StorageSectionData& values) {
    jsonObject = json::object();

    if (values.enabled) {
        jsonObject["enabled"] = *values.enabled;
    }
    if (values.path) {
        jsonObject["path"] = *values.path;
    }
    if (values.format) {
        jsonObject["format"] = *values.format;
    }
}

inline void to_json(json& jsonObject, const StorageData& values) {
    jsonObject = json::object();

    if (values.recording) {
        jsonObject["recording"] = *values.recording;
    }
    if (values.snapshots) {
        jsonObject["snapshots"] = *values.snapshots;
    }
    if (values.logs) {
        jsonObject["logs"] = *values.logs;
    }
    if (values.thumbnails) {
        jsonObject["thumbnails"] = *values.thumbnails;
    }
}

}  // namespace sst::config::domain
