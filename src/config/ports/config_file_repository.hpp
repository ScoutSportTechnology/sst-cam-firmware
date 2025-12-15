#pragma once

#include <utility>

namespace sst::config::ports {

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

template <typename T>
class IConfigFileRepository {
   public:
    virtual ~IConfigFileRepository() = default;

    virtual auto load() -> ConfigReturn<T> = 0;
    virtual auto save(const T& modifiedConfig) -> ConfigReturn<T> = 0;
    virtual auto reset() -> ConfigReturn<T> = 0;
};

}  // namespace sst::config::ports
