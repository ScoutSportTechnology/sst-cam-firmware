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

inline void from_json(const json& jsonObject, DefaultStorageSectionConfig& value) {
    jsonObject.at("enabled").get_to(value.enabled);
    jsonObject.at("path").get_to(value.path);
    jsonObject.at("format").get_to(value.format);
}

inline void to_json(json& jsonObject, const DefaultStorageConfig& value) {
    jsonObject = json::object();
    jsonObject["recording"] = value.recording;
    jsonObject["snapshots"] = value.snapshots;
    jsonObject["logs"] = value.logs;
    jsonObject["thumbnails"] = value.thumbnails;
}

inline void from_json(const json& jsonObject, DefaultStorageConfig& value) {
    jsonObject.at("recording").get_to(value.recording);
    jsonObject.at("snapshots").get_to(value.snapshots);
    jsonObject.at("logs").get_to(value.logs);
    jsonObject.at("thumbnails").get_to(value.thumbnails);
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
