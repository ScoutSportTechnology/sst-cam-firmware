#pragma once

#include <string>

namespace sst::config::ports {
template <typename T>
class ConfigFileRepository {
   public:
    virtual ~ConfigFileRepository() = default;

    virtual bool load_config(T& loadedConfig, std::string& error) = 0;
    virtual bool save_config(const T& modifiedConfig, std::string& error) = 0;
};
}  // namespace sst::config::ports