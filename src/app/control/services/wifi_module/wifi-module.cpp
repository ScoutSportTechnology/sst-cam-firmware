#include "app/control/services/wifi_module/wifi-module.hpp"

#include <utility>

namespace sst::control {

WifiModule::WifiModule(std::unique_ptr<IWifiManager> manager)
    : manager_(std::move(manager)) {}

auto WifiModule::StartP2pGroupOwner(const WifiCredentials& creds) -> bool {
    return manager_->StartP2pGroupOwner(creds);
}

auto WifiModule::Stop() -> void { manager_->Stop(); }

auto WifiModule::State() const -> WifiState { return manager_->State(); }

}  // namespace sst::control
