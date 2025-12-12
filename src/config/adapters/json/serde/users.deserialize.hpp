#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/users_config.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void from_json(const json& jsonObject, UsersUserConfig& value) {
    jsonObject.at("id").get_to(value.id);
    jsonObject.at("name").get_to(value.name);
}

inline void from_json(const json& jsonObject, UsersConfig& value) {
    jsonObject.at("default_data").get_to(value.default_data);
    jsonObject.at("users").get_to(value.users);
}

}  // namespace sst::config::domain
