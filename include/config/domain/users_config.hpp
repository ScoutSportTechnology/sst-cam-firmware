#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sst::config::domain {

struct UsersUserConfig {
    std::uint32_t id{0};
    std::string name;
};

struct UsersConfig {
    std::vector<UsersUserConfig> default_data;
    std::vector<UsersUserConfig> users;
};

}  // namespace sst::config::domain
