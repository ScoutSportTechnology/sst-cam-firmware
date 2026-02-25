#pragma once

#include <optional>
#include <string>

#include "domain/config/models/config-files.hpp"

namespace sst::config::domain {

struct StorageSectionData {
    std::optional<bool> enabled{std::nullopt};
    std::optional<std::string> path{std::nullopt};
    std::optional<std::string> format{std::nullopt};
};

struct StorageData {
    std::optional<StorageSectionData> recording{std::nullopt};
    std::optional<StorageSectionData> snapshots{std::nullopt};
    std::optional<StorageSectionData> logs{std::nullopt};
    std::optional<StorageSectionData> thumbnails{std::nullopt};
};

using StorageConfig = ConfigFiles<StorageData, StorageData>;

}  // namespace sst::config::domain
