#pragma once

#include <optional>

#include "config/domain/config_files.hpp"

namespace sst::config::domain {

struct ProfileData {
    std::optional<bool> calibration{std::nullopt};
    std::optional<bool> device{std::nullopt};
    std::optional<bool> stream{std::nullopt};
    std::optional<bool> storage{std::nullopt};
};

using ProfileConfig = ConfigFiles<ProfileData, ProfileData>;

}  // namespace sst::config::domain
