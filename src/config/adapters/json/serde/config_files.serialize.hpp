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

template <typename DD, typename UD>
inline void to_json(json& jsonObject, const ConfigFiles<DD, UD>& value) {
    jsonObject = json{
        {"default_data", value.default_data},
        {"users", value.users},
    };
}
}  // namespace sst::config::domain