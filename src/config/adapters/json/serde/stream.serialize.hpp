#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/stream_config.hpp"

namespace sst::config::domain {

using nlohmann::json;

inline void to_json(json& jsonObject, const StreamPlatformSettingsData& values) {
    jsonObject = json::object();

    if (values.width) {
        jsonObject["width"] = *values.width;
    }
    if (values.height) {
        jsonObject["height"] = *values.height;
    }
    if (values.framerate) {
        jsonObject["framerate"] = *values.framerate;
    }
    if (values.bitrate_kbps) {
        jsonObject["bitrate_kbps"] = *values.bitrate_kbps;
    }
}

inline void to_json(json& jsonObject, const StreamPlatformData& values) {
    jsonObject = json::object();

    if (values.enabled) {
        jsonObject["enabled"] = *values.enabled;
    }
    if (values.url) {
        jsonObject["url"] = *values.url;
    }
    if (values.key) {
        jsonObject["key"] = *values.key;
    }
    if (values.stream_type) {
        jsonObject["stream_type"] = *values.stream_type;
    }
    if (values.codec) {
        jsonObject["codec"] = *values.codec;
    }
    if (values.settings) {
        jsonObject["settings"] = *values.settings;
    }
}

inline void to_json(json& jsonObject, const StreamData& values) {
    jsonObject = json::object();

    if (values.youtube) {
        jsonObject["youtube"] = *values.youtube;
    }
    if (values.twitch) {
        jsonObject["twitch"] = *values.twitch;
    }
    if (values.facebook) {
        jsonObject["facebook"] = *values.facebook;
    }
    if (values.instagram) {
        jsonObject["instagram"] = *values.instagram;
    }
    if (values.tik_tok) {
        jsonObject["tik_tok"] = *values.tik_tok;
    }
    if (values.custom) {
        jsonObject["custom"] = *values.custom;
    }
}

}  // namespace sst::config::domain
