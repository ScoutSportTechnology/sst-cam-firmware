#pragma once

#include <utility>

namespace sst::config {

template <typename T>
struct ConfigReturn {
    bool success{false};
    T data{};

    static auto ok(T value) -> ConfigReturn {
        ConfigReturn configReturn;
        configReturn.success = true;
        configReturn.data = std::move(value);
        return configReturn;
    }

    static auto fail() -> ConfigReturn { return ConfigReturn{}; }
};

}  // namespace sst::config
