#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace domain::config {

struct UsersUserConfig {
    std::uint32_t id{0};
    std::string name;
};

struct UsersConfig {
    std::vector<UsersUserConfig> default_data;
    std::vector<UsersUserConfig> users;
};

}  // namespace domain::config
