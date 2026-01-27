#pragma once
#include <nlohmann/json.hpp>

#include "domain/config/models/stream.hpp"

namespace sst::config::domain {

using nlohmann::json;

inline void from_json(const json& jsonObject, StreamPlatformSettingsData& values) {
    if (jsonObject.contains("width")) {
        values.width = jsonObject.at("width").get<std::uint32_t>();
    }
    if (jsonObject.contains("height")) {
        values.height = jsonObject.at("height").get<std::uint32_t>();
    }
    if (jsonObject.contains("framerate")) {
        values.framerate = jsonObject.at("framerate").get<std::uint32_t>();
    }
    if (jsonObject.contains("bitrate_kbps")) {
        values.bitrate_kbps = jsonObject.at("bitrate_kbps").get<std::uint32_t>();
    }
}

inline void from_json(const json& jsonObject, StreamPlatformData& values) {
    if (jsonObject.contains("enabled")) {
        values.enabled = jsonObject.at("enabled").get<bool>();
    }
    if (jsonObject.contains("url")) {
        values.url = jsonObject.at("url").get<std::string>();
    }
    if (jsonObject.contains("key")) {
        values.key = jsonObject.at("key").get<std::string>();
    }
    if (jsonObject.contains("stream_type")) {
        values.stream_type = jsonObject.at("stream_type").get<std::string>();
    }
    if (jsonObject.contains("codec")) {
        values.codec = jsonObject.at("codec").get<std::string>();
    }
    if (jsonObject.contains("settings")) {
        values.settings = jsonObject.at("settings").get<StreamPlatformSettingsData>();
    }
}

inline void from_json(const json& jsonObject, StreamData& values) {
    if (jsonObject.contains("youtube")) {
        values.youtube = jsonObject.at("youtube").get<StreamPlatformData>();
    }
    if (jsonObject.contains("twitch")) {
        values.twitch = jsonObject.at("twitch").get<StreamPlatformData>();
    }
    if (jsonObject.contains("facebook")) {
        values.facebook = jsonObject.at("facebook").get<StreamPlatformData>();
    }
    if (jsonObject.contains("instagram")) {
        values.instagram = jsonObject.at("instagram").get<StreamPlatformData>();
    }
    if (jsonObject.contains("tik_tok")) {
        values.tik_tok = jsonObject.at("tik_tok").get<StreamPlatformData>();
    }
    if (jsonObject.contains("custom")) {
        values.custom = jsonObject.at("custom").get<std::vector<StreamPlatformData>>();
    }
}

}  // namespace sst::config::domain
