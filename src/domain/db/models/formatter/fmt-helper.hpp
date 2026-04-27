#pragma once

#include <fmt/format.h>

#include <optional>
#include <string>

namespace sst::db {

inline auto BoolToStr(bool value) -> const char* { return value ? "true" : "false"; }

inline auto StrOptToStr(const std::optional<std::string>& values) -> std::string {
    if (!values) {
        return "null";
    }
    return fmt::format("\"{}\"", *values);
}

}  // namespace sst::db
