#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/stream_config.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void to_json(json& jsonObject, const DefaultStreamPlatformSettingsConfig& value) {
    jsonObject = json::object();
    jsonObject["width"] = value.width;
    jsonObject["height"] = value.height;
    jsonObject["framerate"] = value.framerate;
    jsonObject["bitrate_kbps"] = value.bitrate_kbps;
}

inline void to_json(json& jsonObject, const DefaultStreamPlatformConfig& value) {
    jsonObject = json::object();
    jsonObject["enabled"] = value.enabled;
    jsonObject["url"] = value.url;
    jsonObject["key"] = value.key;
    jsonObject["stream_type"] = value.stream_type;
    jsonObject["codec"] = value.codec;
    jsonObject["settings"] = value.settings;
}

inline void to_json(json& jsonObject, const DefaultStreamConfig& value) {
    jsonObject = json::object();
    jsonObject["youtube"] = value.youtube;
    jsonObject["twitch"] = value.twitch;
    jsonObject["facebook"] = value.facebook;
    jsonObject["instagram"] = value.instagram;
    jsonObject["tik_tok"] = value.tik_tok;
    jsonObject["custom"] = value.custom;
}

inline void to_json(json& jsonObject, const UserStreamPlatformSettingsConfig& value) {
    jsonObject = json::object();
    if (value.width.has_value()) {
        jsonObject["width"] = value.width.value();
    }
    if (value.height.has_value()) {
        jsonObject["height"] = value.height.value();
    }
    if (value.framerate.has_value()) {
        jsonObject["framerate"] = value.framerate.value();
    }
    if (value.bitrate_kbps.has_value()) {
        jsonObject["bitrate_kbps"] = value.bitrate_kbps.value();
    }
}

inline void to_json(json& jsonObject, const UserStreamPlatformConfig& value) {
    jsonObject = json::object();
    if (value.enabled.has_value()) {
        jsonObject["enabled"] = value.enabled.value();
    }
    if (value.url.has_value()) {
        jsonObject["url"] = value.url.value();
    }
    if (value.key.has_value()) {
        jsonObject["key"] = value.key.value();
    }
    if (value.stream_type.has_value()) {
        jsonObject["stream_type"] = value.stream_type.value();
    }
    if (value.codec.has_value()) {
        jsonObject["codec"] = value.codec.value();
    }
    if (value.settings.has_value()) {
        jsonObject["settings"] = value.settings.value();
    }
}

inline void to_json(json& jsonObject, const UserStreamConfig& value) {
    jsonObject = json::object();
    if (value.youtube.has_value()) {
        jsonObject["youtube"] = value.youtube.value();
    }
    if (value.twitch.has_value()) {
        jsonObject["twitch"] = value.twitch.value();
    }
    if (value.facebook.has_value()) {
        jsonObject["facebook"] = value.facebook.value();
    }
    if (value.instagram.has_value()) {
        jsonObject["instagram"] = value.instagram.value();
    }
    if (value.tik_tok.has_value()) {
        jsonObject["tik_tok"] = value.tik_tok.value();
    }
    if (value.custom.has_value()) {
        jsonObject["custom"] = value.custom.value();
    }
}

}  // namespace sst::config::domain
