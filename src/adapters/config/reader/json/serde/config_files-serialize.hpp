#pragma once
#include <nlohmann/json.hpp>

#include "domain/config/models/config-files.hpp"

namespace sst::config::domain {
using nlohmann::json;

template <typename UD>
inline void to_json(json& jsonObject, const UserArray<UD>& values) {
    jsonObject = json{
        {"id", values.id},
        {"user_data", values.user_data},
    };
}

template <typename DD, typename UD>
inline void to_json(json& jsonObject, const ConfigFiles<DD, UD>& values) {
    jsonObject = json{
        {"default_data", values.default_data},
        {"users", values.users},
    };
}

}  // namespace sst::config::domain
