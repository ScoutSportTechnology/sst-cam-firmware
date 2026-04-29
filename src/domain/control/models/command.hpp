#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sst::control {

// Opaque command envelope dispatched from a transport (BLE / WiFi) to a
// registered controller. `route` selects the IController by its Id() ("camera",
// "recording", ...). `payload` holds raw bytes which the controller decodes
// itself; once the .proto schema is fixed, payload becomes a typed message.
struct Command {
    std::string route;
    std::vector<std::byte> payload;
    std::uint64_t correlation_id{0};
};

}  // namespace sst::control
