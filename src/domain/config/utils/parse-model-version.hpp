#pragma once

#include <cctype>
#include <charconv>
#include <optional>
#include <string_view>

namespace sst::config {

// Parses a device model string of the form "v<N>" or "V<N>" into the integer N.
// Returns nullopt for empty input, missing 'v' prefix, or non-numeric suffix.
inline auto ParseModelVersion(std::string_view model) -> std::optional<int> {
    if (model.size() < 2) {
        return std::nullopt;
    }
    if (model.front() != 'v' && model.front() != 'V') {
        return std::nullopt;
    }
    const std::string_view digits = model.substr(1);
    int value = 0;
    const auto* begin = digits.data();
    const auto* end = digits.data() + digits.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return value;
}

}  // namespace sst::config
