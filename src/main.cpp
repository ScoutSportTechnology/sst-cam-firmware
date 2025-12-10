#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "config/adapters/json_adapter.hpp"
#include "config/domain/config_files.hpp"
#include "config/domain/profile_config.hpp"

auto main() -> int {
    using sst::config::adapters::JsonAdapter;
    using sst::config::domain::ProfileConfig;
    using sst::config::domain::UserArray;
    using sst::config::domain::UserProfileConfig;

    JsonAdapter<ProfileConfig> jsonAdapter{"profile"};

    ProfileConfig profileConfig{};
    std::string error;

    if (!jsonAdapter.load(profileConfig, error)) {
        std::cerr << "Failed to load profile config: " << error << '\n';
        return 1;
    }

    std::cout << "Loaded profile config\n";
    std::cout << "[default_data]\n";
    std::cout << "  calibration: " << profileConfig.default_data.calibration << '\n';
    std::cout << "  device:      " << profileConfig.default_data.device << '\n';
    std::cout << "  stream:      " << profileConfig.default_data.stream << '\n';
    std::cout << "  storage:     " << profileConfig.default_data.storage << '\n';
    std::cout << "\n[users]\n";
    std::cout << "  count: " << profileConfig.users.size() << '\n';

    for (const UserArray<UserProfileConfig>& userEntry : profileConfig.users) {
        std::cout << "  user id: " << userEntry.id << '\n';
        const UserProfileConfig& u = userEntry.user_data;

        if (u.calibration.has_value()) {
            std::cout << "    calibration: " << *u.calibration << '\n';
        }
        if (u.device.has_value()) {
            std::cout << "    device:      " << *u.device << '\n';
        }
        if (u.stream.has_value()) {
            std::cout << "    stream:      " << *u.stream << '\n';
        }
        if (u.storage.has_value()) {
            std::cout << "    storage:     " << *u.storage << '\n';
        }
    }

    return 0;
}
