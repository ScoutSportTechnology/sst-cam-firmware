#pragma once

#include <optional>

#include "config/domain/config_files.hpp"

namespace sst::config::domain  {

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

}  // namespace sst::config::domain
