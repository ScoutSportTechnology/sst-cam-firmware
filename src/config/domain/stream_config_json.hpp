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

inline void from_json(const json& jsonObject, DefaultStreamPlatformSettingsConfig& value) {
    jsonObject.at("width").get_to(value.width);
    jsonObject.at("height").get_to(value.height);
    jsonObject.at("framerate").get_to(value.framerate);
    jsonObject.at("bitrate_kbps").get_to(value.bitrate_kbps);
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

inline void from_json(const json& jsonObject, DefaultStreamPlatformConfig& value) {
    jsonObject.at("enabled").get_to(value.enabled);
    jsonObject.at("url").get_to(value.url);
    jsonObject.at("key").get_to(value.key);
    jsonObject.at("stream_type").get_to(value.stream_type);
    jsonObject.at("codec").get_to(value.codec);
    jsonObject.at("settings").get_to(value.settings);
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

inline void from_json(const json& jsonObject, DefaultStreamConfig& value) {
    jsonObject.at("youtube").get_to(value.youtube);
    jsonObject.at("twitch").get_to(value.twitch);
    jsonObject.at("facebook").get_to(value.facebook);
    jsonObject.at("instagram").get_to(value.instagram);
    jsonObject.at("tik_tok").get_to(value.tik_tok);
    jsonObject.at("custom").get_to(value.custom);
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

inline void from_json(const json& jsonObject, UserStreamPlatformSettingsConfig& value) {
    if (jsonObject.contains("width")) {
        value.width = jsonObject.at("width").get<std::uint32_t>();
    }
    if (jsonObject.contains("height")) {
        value.height = jsonObject.at("height").get<std::uint32_t>();
    }
    if (jsonObject.contains("framerate")) {
        value.framerate = jsonObject.at("framerate").get<std::uint32_t>();
    }
    if (jsonObject.contains("bitrate_kbps")) {
        value.bitrate_kbps = jsonObject.at("bitrate_kbps").get<std::uint32_t>();
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

inline void from_json(const json& jsonObject, UserStreamPlatformConfig& value) {
    if (jsonObject.contains("enabled")) {
        value.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("url")) {
        value.url = jsonObject.at("url").get<std::string>();
    }
    if (jsonObject.contains("key")) {
        value.key = jsonObject.at("key").get<std::string>();
    }
    if (jsonObject.contains("stream_type")) {
        value.stream_type = jsonObject.at("stream_type").get<std::string>();
    }
    if (jsonObject.contains("codec")) {
        value.codec = jsonObject.at("codec").get<std::string>();
    }
    if (jsonObject.contains("settings")) {
        value.settings = jsonObject.at("settings").get<UserStreamPlatformSettingsConfig>();
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

inline void from_json(const json& jsonObject, UserStreamConfig& value) {
    if (jsonObject.contains("youtube")) {
        value.youtube = jsonObject.at("youtube").get<UserStreamPlatformConfig>();
    }
    if (jsonObject.contains("twitch")) {
        value.twitch = jsonObject.at("twitch").get<UserStreamPlatformConfig>();
    }
    if (jsonObject.contains("facebook")) {
        value.facebook = jsonObject.at("facebook").get<UserStreamPlatformConfig>();
    }
    if (jsonObject.contains("instagram")) {
        value.instagram = jsonObject.at("instagram").get<UserStreamPlatformConfig>();
    }
    if (jsonObject.contains("tik_tok")) {
        value.tik_tok = jsonObject.at("tik_tok").get<UserStreamPlatformConfig>();
    }
    if (jsonObject.contains("custom")) {
        value.custom = jsonObject.at("custom").get<std::vector<UserStreamPlatformConfig>>();
    }
}

}  // namespace sst::config::domain
