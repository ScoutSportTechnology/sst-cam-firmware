#include "config/domain/users_config.hpp"
#include "config/ports/config_file_adapter.hpp"
#include "config/ports/config_file_loader.hpp"

namespace sst::config::app {
using sst::config::domain::UsersConfig;
using sst::config::domain::UsersData;
using sst::config::ports::IConfigFileAdapter;
using sst::config::ports::IConfigFileLoader;

class UserLoader : public IConfigFileLoader<UsersData, UsersConfig> {
   public:
    explicit UserLoader(IConfigFileAdapter<UsersConfig> adapter);
    auto get() -> UsersData override{
      
    };
};
}  // namespace sst::config::app