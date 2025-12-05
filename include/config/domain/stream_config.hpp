#pragma once

#include <optional>
#include <string>
#include <vector>

#include "config/domain/config_files.hpp"

namespace domain::config {

struct DefaultStreamPlatformSettingsConfig {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t framerate{0};
    std::uint32_t bitrate_kbps{0};
};

struct DefaultStreamPlatformConfig {
    bool enabled{false};
    std::string url;
    std::string key;
    std::string stream_type;
    std::string codec;
    DefaultStreamPlatformSettingsConfig settings;
};

struct DefaultStreamConfig {
    DefaultStreamPlatformConfig youtube;
    DefaultStreamPlatformConfig twitch;
    DefaultStreamPlatformConfig facebook;
    DefaultStreamPlatformConfig instagram;
    DefaultStreamPlatformConfig tik_tok;
    std::vector<DefaultStreamPlatformConfig> custom;
};

struct UserStreamPlatformSettingsConfig {
    std::optional<std::uint32_t> width;
    std::optional<std::uint32_t> height;
    std::optional<std::uint32_t> framerate;
    std::optional<std::uint32_t> bitrate_kbps;
};

struct UserStreamPlatformConfig {
    std::optional<bool> enabled;
    std::optional<std::string> url;
    std::optional<std::string> key;
    std::optional<std::string> stream_type;
    std::optional<std::string> codec;
    std::optional<UserStreamPlatformSettingsConfig> settings;
};

struct UserStreamConfig {
    std::optional<UserStreamPlatformConfig> youtube;
    std::optional<UserStreamPlatformConfig> twitch;
    std::optional<UserStreamPlatformConfig> facebook;
    std::optional<UserStreamPlatformConfig> instagram;
    std::optional<UserStreamPlatformConfig> tik_tok;
    std::optional<std::vector<UserStreamPlatformConfig>> custom;
};

using StreamConfig = ConfigFiles<DefaultStreamConfig, UserStreamConfig>;

}  // namespace domain::config
