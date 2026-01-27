#pragma once
#include <nlohmann/json.hpp>

#include "domain/config/models/users.hpp"

namespace sst::config::domain {
using nlohmann::json;
inline void to_json(json& jsonObject, const UsersData& values) {
    jsonObject = json{
        {"name", values.name},
    };
}
}  // namespace sst::config::domain
