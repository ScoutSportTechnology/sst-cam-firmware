#pragma once

#include "domain/config/models/config-return.hpp"

namespace sst::config {

using sst::config::ConfigReturn;

template <typename T>
class IConfigFileReaderAdapter {
   public:
    virtual ~IConfigFileReaderAdapter() = default;

    virtual auto load() -> ConfigReturn<T> = 0;
};

}  // namespace sst::config
