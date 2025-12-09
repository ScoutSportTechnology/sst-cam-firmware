#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "config/domain/profile_config.hpp"
#include "config/adapters/json_adapter.hpp"

int main() {
    using sst::config::adapters::JsonAdapter;
    using sst::config::domain::ProfileConfig;

    JsonAdapter<ProfileConfig> jsonAdapter{"config/profile.json"};

    ProfileConfig profileConfig{};
    std::string error;

    if (!jsonAdapter.load(profileConfig, error)) {
        std::cerr << "Failed to load profile config: " << error << '\n';
        return 1;
    }

    std::cout << "Loaded profile config\n";
    //std::cout << "default_data size: " << profileConfig.default_data.size() << '\n';
    std::cout << "users size:        " << profileConfig.users.size() << '\n';

    return 0;
}
