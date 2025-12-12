#pragma once
#include <nlohmann/json.hpp>

#include "config/domain/users_config.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void from_json(const json& jsonObject, UsersData& values) {
    jsonObject.at("name").get_to(values.name);
}

}  // namespace sst::config::domain
