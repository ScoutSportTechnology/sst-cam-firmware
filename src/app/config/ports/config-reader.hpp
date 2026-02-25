#pragma once


#include "domain/config/models/config-return.hpp"

namespace sst::config::ports {

using sst::config::domain::ConfigReturn;

template <typename T>
class IConfigFileReaderAdapter {
   public:
    virtual ~IConfigFileReaderAdapter() = default;

    virtual auto load() -> ConfigReturn<T> = 0;
};

}  // namespace sst::config::ports
