#pragma once
#include <nlohmann/json.hpp>
#include "config/domain/config_files.hpp"

namespace sst::config::domain {
using nlohmann::json;

template <typename UD>
inline void to_json(json& jsonObject, const UserArray<UD>& value) {
    jsonObject = json{
        {"id", value.id},
        {"user_data", value.user_data},
    };
}

template <typename UD>
inline void from_json(const json& jsonObject, UserArray<UD>& value) {
    jsonObject.at("id").get_to(value.id);
    jsonObject.at("user_data").get_to(value.user_data);
}


template <typename DD, typename UD>
inline void to_json(json& jsonObject, const ConfigFiles<DD, UD>& value) {
    jsonObject = json{
        {"default_data", value.default_data},
        {"users", value.users},
    };
}
template <typename DD, typename UD>
inline void from_json(const json& jsonObject, ConfigFiles<DD, UD>& value) {
    jsonObject.at("default_data").get_to(value.default_data);
    jsonObject.at("users").get_to(value.users);
}
}  // namespace sst::config::domain