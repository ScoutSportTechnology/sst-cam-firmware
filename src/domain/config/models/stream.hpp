#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "domain/config/models/config-files.hpp"

namespace sst::config {

struct StreamPlatformSettingsData {
    std::optional<std::uint32_t> width{std::nullopt};
    std::optional<std::uint32_t> height{std::nullopt};
    std::optional<std::uint32_t> framerate{std::nullopt};
    std::optional<std::uint32_t> bitrate_kbps{std::nullopt};
};

struct StreamPlatformData {
    std::optional<bool> enabled{std::nullopt};
    std::optional<std::string> url{std::nullopt};
    std::optional<std::string> key{std::nullopt};
    std::optional<std::string> stream_type{std::nullopt};
    std::optional<std::string> codec{std::nullopt};
    std::optional<StreamPlatformSettingsData> settings{std::nullopt};
};

struct StreamData {
    std::optional<StreamPlatformData> youtube{std::nullopt};
    std::optional<StreamPlatformData> twitch{std::nullopt};
    std::optional<StreamPlatformData> facebook{std::nullopt};
    std::optional<StreamPlatformData> instagram{std::nullopt};
    std::optional<StreamPlatformData> tik_tok{std::nullopt};
    std::optional<std::vector<StreamPlatformData>> custom{std::nullopt};
};

using StreamConfig = ConfigFiles<StreamData, StreamData>;

}  // namespace sst::config
