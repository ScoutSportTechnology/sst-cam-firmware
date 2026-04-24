#pragma once

#include <nlohmann/json.hpp>
#include <optional>

namespace sst::config {

template <typename T>
void json_set_opt(nlohmann::json& obj, const char* key, const std::optional<T>& val) {
    if (val) {
        obj[key] = *val;
    }
}

template <typename T>
void json_get_opt(const nlohmann::json& obj, const char* key, std::optional<T>& val) {
    if (obj.contains(key)) {
        val = obj.at(key).get<T>();
    }
}

}  // namespace sst::config
