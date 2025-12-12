#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/storage_config.hpp"

namespace sst::config::domain {

using nlohmann::json;

inline void from_json(const json& jsonObject, StorageSectionData& values) {
    if (jsonObject.contains("enabled")) {
        values.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("path")) {
        values.path = jsonObject.at("path").get<std::string>();
    }
    if (jsonObject.contains("format")) {
        values.format = jsonObject.at("format").get<std::string>();
    }
}

inline void from_json(const json& jsonObject, StorageData& values) {
    if (jsonObject.contains("recording")) {
        values.recording = jsonObject.at("recording").get<StorageSectionData>();
    }
    if (jsonObject.contains("snapshots")) {
        values.snapshots = jsonObject.at("snapshots").get<StorageSectionData>();
    }
    if (jsonObject.contains("logs")) {
        values.logs = jsonObject.at("logs").get<StorageSectionData>();
    }
    if (jsonObject.contains("thumbnails")) {
        values.thumbnails = jsonObject.at("thumbnails").get<StorageSectionData>();
    }
}

}  // namespace sst::config::domain
