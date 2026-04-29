#pragma once

#include <memory>

#include "app/control/ports/wifi-manager.hpp"

namespace sst::control {

// Thin facade over IWifiManager. Exists so callers depend on a stable app-layer
// type rather than the port directly, matching the DbManager / ConfigLoader
// pattern. Future enhancements (retry/backoff, credential caching, state
// notifications to controllers) belong here, not in the adapter.
class WifiModule {
   public:
    explicit WifiModule(std::unique_ptr<IWifiManager> manager);

    auto StartClient(const WifiCredentials& creds) -> bool;
    auto StartAccessPoint(const WifiCredentials& creds) -> bool;
    auto StartP2pGroupOwner(const WifiCredentials& creds) -> bool;
    auto Stop() -> void;
    auto State() const -> WifiState;

    auto manager() -> IWifiManager& { return *manager_; }

   private:
    std::unique_ptr<IWifiManager> manager_;
};

}  // namespace sst::control
