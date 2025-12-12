#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/users_config.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void to_json(nlohmann::json& jsonObject, const UsersUserConfig& value) {
    jsonObject = json{
        {"id", value.id},
        {"name", value.name},
    };
}

inline void to_json(json& jsonObject, const UsersConfig& value) {
    jsonObject = json{
        {"default_data", value.default_data},
        {"users", value.users},
    };
}

}  // namespace sst::config::domain
