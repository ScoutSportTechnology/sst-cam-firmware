#pragma once

#include <optional>
#include <string>

#include "config/domain/config_files.hpp"

namespace sst::config::domain  {

struct DefaultStorageSectionConfig {
    bool enabled{false};
    std::string path;
    std::string format;
};

struct DefaultStorageConfig {
    DefaultStorageSectionConfig recording;
    DefaultStorageSectionConfig snapshots;
    DefaultStorageSectionConfig logs;
    DefaultStorageSectionConfig thumbnails;
};

struct UserStorageSectionConfig {
    std::optional<bool> enabled;
    std::optional<std::string> path;
    std::optional<std::string> format;
};

struct UserStorageConfig {
    std::optional<UserStorageSectionConfig> recording;
    std::optional<UserStorageSectionConfig> snapshots;
    std::optional<UserStorageSectionConfig> logs;
    std::optional<UserStorageSectionConfig> thumbnails;
};

using StorageConfig = ConfigFiles<DefaultStorageConfig, UserStorageConfig>;

}  // namespace sst::config::domain
