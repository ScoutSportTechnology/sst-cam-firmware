#pragma once

#include <optional>

#include "domain/config/config_files.hpp"

namespace domain::config {

struct DefaultProfileConfig {
    bool calibration{false};
    bool device{false};
    bool stream{false};
    bool storage{false};
};

struct UserProfileConfig {
    std::optional<bool> calibration;
    std::optional<bool> device;
    std::optional<bool> stream;
    std::optional<bool> storage;
};

using ProfileConfig = ConfigFiles<DefaultProfileConfig, UserProfileConfig>;

}  // namespace domain::config
