#pragma once

#include "domain/config/models/config-return.hpp"

namespace sst::config {

using sst::config::ConfigReturn;

template <typename T>
class IConfigFileWriterAdapter {
   public:
    virtual ~IConfigFileWriterAdapter() = default;

    virtual auto save(const T& modifiedConfig) -> ConfigReturn<T> = 0;
    virtual auto reset() -> ConfigReturn<T> = 0;
};

}  // namespace sst::config
