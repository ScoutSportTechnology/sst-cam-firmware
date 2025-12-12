#pragma once

#include <string>
#include <vector>

#include "config/domain/config_files.hpp"

namespace sst::config::domain {
struct UsersData {
    std::string name;
};

using UsersConfig = ConfigFiles<std::vector<UsersData>, UsersData>;
}  // namespace sst::config::domain
