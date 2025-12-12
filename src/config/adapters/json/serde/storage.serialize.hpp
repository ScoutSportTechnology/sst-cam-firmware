#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/storage_config.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void to_json(json& jsonObject, const DefaultStorageSectionConfig& value) {
    jsonObject = json::object();
    jsonObject["enabled"] = value.enabled;
    jsonObject["path"] = value.path;
    jsonObject["format"] = value.format;
}


inline void to_json(json& jsonObject, const DefaultStorageConfig& value) {
    jsonObject = json::object();
    jsonObject["recording"] = value.recording;
    jsonObject["snapshots"] = value.snapshots;
    jsonObject["logs"] = value.logs;
    jsonObject["thumbnails"] = value.thumbnails;
}

inline void to_json(json& jsonObject, const UserStorageSectionConfig& value) {
    jsonObject = json::object();
    if (value.enabled.has_value()) {
        jsonObject["enabled"] = value.enabled.value();
    }
    if (value.path.has_value()) {
        jsonObject["path"] = value.path.value();
    }
    if (value.format.has_value()) {
        jsonObject["format"] = value.format.value();
    }
}

inline void to_json(json& jsonObject, const UserStorageConfig& value) {
    jsonObject = json::object();
    if (value.recording.has_value()) {
        jsonObject["recording"] = value.recording.value();
    }
    if (value.snapshots.has_value()) {
        jsonObject["snapshots"] = value.snapshots.value();
    }
    if (value.logs.has_value()) {
        jsonObject["logs"] = value.logs.value();
    }
    if (value.thumbnails.has_value()) {
        jsonObject["thumbnails"] = value.thumbnails.value();
    }
}


}  // namespace sst::config::domain
