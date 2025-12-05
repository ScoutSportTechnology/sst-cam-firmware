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

inline void from_json(const json& jsonObject, UsersUserConfig& value) {
    jsonObject.at("id").get_to(value.id);
    jsonObject.at("name").get_to(value.name);
}


inline void to_json(json& jsonObject, const UsersConfig& value) {
    jsonObject = json{
        {"default_data", value.default_data},
        {"users", value.users},
    };
}

inline void from_json(const json& jsonObject, UsersConfig& value) {
    jsonObject.at("default_data").get_to(value.default_data);
    jsonObject.at("users").get_to(value.users);
}

}  // namespace sst::config::domain
