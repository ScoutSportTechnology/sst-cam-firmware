#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace sst::adapters::control {

class StaleBondRecovery {
   public:
    using Clock = std::chrono::steady_clock;
    struct Link {
        std::string path;
        std::string address;
        std::uint8_t type;
        bool paired;
        bool connected;
    };

    auto Observe(const Link& link) -> void {
        auto& device = devices_[link.path];
        if (!link.connected && device.connected) {
            device.recently_connected = Clock::now();
        }
        device.address = link.address;
        device.type = link.type;
        device.paired = link.paired;
        device.connected = link.connected;
        if (link.connected) {
            device.recently_connected = Clock::now();
        } else if (!link.paired) {
            device.recently_connected.reset();
        }
    }

    auto Forget(const std::string& path) -> void { devices_.erase(path); }
    auto ClearDevices() -> void { devices_.clear(); }

    struct Failure {
        std::string address;
        std::uint8_t type;
        std::uint8_t status;
    };
    static constexpr std::uint8_t kAuthenticationFailed = 0x05;
    static constexpr auto kCooldown = std::chrono::seconds{60};

    auto AuthenticationFailed(const Failure& failure,
                              Clock::time_point now) -> std::optional<std::string> {
        if (failure.status != kAuthenticationFailed) {
            return std::nullopt;
        }
        for (const auto& [path, device] : devices_) {
            if (device.address != failure.address || device.type != failure.type ||
                !device.paired ||
                (!device.connected &&
                 (!device.recently_connected || now - *device.recently_connected > kCooldown))) {
                continue;
            }
            const auto last = removed_.find(failure.address);
            if (last != removed_.end() && now - last->second < kCooldown) {
                return std::nullopt;
            }
            removed_[failure.address] = now;
            return path;
        }
        return std::nullopt;
    }

   private:
    struct Device {
        std::string address;
        std::uint8_t type;
        bool paired;
        bool connected{false};
        std::optional<Clock::time_point> recently_connected;
    };
    std::map<std::string, Device> devices_;
    std::map<std::string, Clock::time_point> removed_;
};

}  // namespace sst::adapters::control
