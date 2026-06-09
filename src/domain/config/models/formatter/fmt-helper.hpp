#pragma once

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace sst::config {

inline auto BoolOptToStr(const std::optional<bool>& values) -> const char* {
    if (!values) {
        return "null";
    }
    return *values ? "true" : "false";
}

inline auto StrOptToStr(const std::optional<std::string>& values) -> std::string {
    if (!values) {
        return "null";
    }
    return fmt::format("\"{}\"", *values);
}

template <typename T>
inline auto NumOptToStr(const std::optional<T>& values) -> std::string {
    if (!values) {
        return "null";
    }
    return fmt::format("{}", *values);
}

template <typename T, std::size_t N>
inline auto ArrOptToStr(const std::optional<std::array<T, N>>& values) -> std::string {
    if (!values) {
        return "null";
    }
    return fmt::format("[{}]", fmt::join(*values, ","));
}

template <typename T>
inline auto VecOptToStr(const std::optional<std::vector<T>>& values) -> std::string {
    if (!values) {
        return "null";
    }
    return fmt::format("[{}]", fmt::join(*values, ","));
}

template <typename T>
inline auto ObjOptToStr(const std::optional<T>& values) -> std::string {
    if (!values) {
        return "null";
    }
    return fmt::format("{}", *values);
}

}  // namespace sst::config
