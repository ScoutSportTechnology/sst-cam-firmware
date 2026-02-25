#pragma once

#include <string>
#include <vector>

#include "domain/config/models/config-files.hpp"

namespace sst::config::domain {
struct UsersData {
    std::string name;
};

using UsersConfig = ConfigFiles<std::vector<UsersData>, UsersData>;
}  // namespace sst::config::domain
