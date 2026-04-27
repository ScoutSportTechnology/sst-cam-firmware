#pragma once

#include <optional>
#include <string>

namespace sst::config {

struct StorageData {
    std::optional<std::string> log{std::nullopt};
    std::optional<std::string> video{std::nullopt};
    std::optional<std::string> snapshots{std::nullopt};
    std::optional<std::string> thumbnails{std::nullopt};
};

}  // namespace sst::config
