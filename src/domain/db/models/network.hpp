#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace sst::db {

struct NetworkWifiDirect {
    static constexpr int32_t kDefaultChannel = 6;

    int64_t user_id{0};
    bool enabled{true};
    std::string ssid;
    std::string passphrase;
    int32_t channel{kDefaultChannel};
    std::optional<std::string> ip_address;
};

struct NetworkBluetooth {
    int64_t user_id{0};
    bool enabled{true};
    std::string name;
    std::string password;
};

}  // namespace sst::db
