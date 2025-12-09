#pragma once

#include <string>

namespace sst::config::ports {
template <typename T>
class IConfigFileRepository {
   public:
    virtual ~IConfigFileRepository() = default;

    virtual auto load(T& loadedConfig, std::string& error) -> bool = 0;
    virtual auto save(const T& modifiedConfig, std::string& error) -> bool = 0;
};
}  // namespace sst::config::ports