#pragma once
#include "config/ports/config_file_adapter.hpp"
namespace sst::config::ports {
using sst::config::ports::IConfigFileAdapter;

template <typename T, typename CF>
class IConfigFileLoader {
   public:
    IConfigFileLoader(IConfigFileAdapter<CF> adapter);
    virtual ~IConfigFileLoader() = default;

    virtual auto get() -> T = 0;
};

}  // namespace sst::config::ports
