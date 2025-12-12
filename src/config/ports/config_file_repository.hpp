#pragma once

namespace sst::config::ports {
template <typename T>
class IConfigFileRepository {
   public:
    virtual ~IConfigFileRepository() = default;

    virtual auto load(T& loadedConfig) -> bool = 0;
    virtual auto save(const T& modifiedConfig) -> bool = 0;
};
}  // namespace sst::config::ports