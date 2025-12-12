#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/storage_config.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void from_json(const json& jsonObject, DefaultStorageSectionConfig& value) {
    jsonObject.at("enabled").get_to(value.enabled);
    jsonObject.at("path").get_to(value.path);
    jsonObject.at("format").get_to(value.format);
}

inline void from_json(const json& jsonObject, DefaultStorageConfig& value) {
    jsonObject.at("recording").get_to(value.recording);
    jsonObject.at("snapshots").get_to(value.snapshots);
    jsonObject.at("logs").get_to(value.logs);
    jsonObject.at("thumbnails").get_to(value.thumbnails);
}

inline void from_json(const json& jsonObject, UserStorageSectionConfig& value) {
    if (jsonObject.contains("enabled")) {
        value.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("path")) {
        value.path = jsonObject.at("path").get<std::string>();
    }
    if (jsonObject.contains("format")) {
        value.format = jsonObject.at("format").get<std::string>();
    }
}

inline void from_json(const json& jsonObject, UserStorageConfig& value) {
    if (jsonObject.contains("recording")) {
        value.recording = jsonObject.at("recording").get<UserStorageSectionConfig>();
    }
    if (jsonObject.contains("snapshots")) {
        value.snapshots = jsonObject.at("snapshots").get<UserStorageSectionConfig>();
    }
    if (jsonObject.contains("logs")) {
        value.logs = jsonObject.at("logs").get<UserStorageSectionConfig>();
    }
    if (jsonObject.contains("thumbnails")) {
        value.thumbnails = jsonObject.at("thumbnails").get<UserStorageSectionConfig>();
    }
}

}  // namespace sst::config::domain
