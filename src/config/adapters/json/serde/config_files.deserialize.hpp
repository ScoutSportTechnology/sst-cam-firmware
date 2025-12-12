#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/config_files.hpp"

namespace sst::config::domain {
using nlohmann::json;

template <typename UD>
inline void from_json(const json& jsonObject, UserArray<UD>& values) {
    jsonObject.at("id").get_to(values.id);
    jsonObject.at("user_data").get_to(values.user_data);
}

template <typename DD, typename UD>
inline void from_json(const json& jsonObject, ConfigFiles<DD, UD>& values) {
    jsonObject.at("default_data").get_to(values.default_data);
    jsonObject.at("users").get_to(values.users);
}

}  // namespace sst::config::domain
