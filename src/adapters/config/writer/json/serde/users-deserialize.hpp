#pragma once
#include <nlohmann/json.hpp>

#include "domain/config/models/users.hpp"

namespace sst::config::domain {
using nlohmann::json;

inline void from_json(const json& jsonObject, UsersData& values) {
    jsonObject.at("name").get_to(values.name);
}

}  // namespace sst::config::domain
