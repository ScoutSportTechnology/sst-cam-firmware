#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>

#include "app/control/ports/wifi-manager.hpp"
#include "app/network/ports/connectivity-arbiter.hpp"
#include "app/network/ports/self-revert-timer.hpp"
#include "app/network/ports/uplink-configurator.hpp"
#include "domain/config/models/uplink-config.hpp"
#include "domain/network/models/connectivity-mode.hpp"

namespace sst::network {

// Concrete arbiter (KTD3/KTD5). Holds the current mode + monotonic migration id
// and is the SOLE symbol that calls both IWifiManager::Stop (GO down) and
// IUplinkConfigurator::ApplyWifiSta (STA up) — sequencing GO-down strictly before
// STA-up so INV2 holds by construction. Every public method serialises on `mtx_`,
// so re-entrant events during kMigrating are handled one at a time and never open
// a concurrent-radio window.
class ConnectivityArbiter final : public IConnectivityArbiter {
   public:
    // `revert_after` is the self-revert backstop window (OQ1): strictly longer
    // than a normal app probe round-trip. Correctness rests on the epoch guard,
    // not this value.
    ConnectivityArbiter(sst::control::IWifiManager& wifi, IUplinkConfigurator& uplink,
                        ISelfRevertTimer& timer, IConnectivityTransitionHooks& hooks,
                        std::chrono::milliseconds revert_after);

    auto RequestSharedWifi(const sst::config::WifiStaUplink& uplink)
        -> std::optional<SharedWifiMigration> override;
    auto ConfirmSharedWifi(MigrationId id, bool reachable) -> ConnectivityMode override;
    auto RevertToP2p() -> void override;
    [[nodiscard]] auto CurrentMode() const -> ConnectivityMode override;

    // Accessors for the network command handler (U4) to build NetworkConfigResponse.
    [[nodiscard]] auto CurrentMigrationId() const -> MigrationId;
    [[nodiscard]] auto PreviewEndpoint() const -> std::string;
    [[nodiscard]] auto PreviewCredential() const -> std::string;

   private:
    // Returns the radio to the GO and resets migration state. Caller holds `mtx_`.
    auto DoRevertLocked() -> void;
    // Self-revert timer expiry — no-op unless still kMigrating on the same epoch.
    auto OnSelfRevertTimeout(MigrationId id) -> void;

    sst::control::IWifiManager& wifi_;
    IUplinkConfigurator& uplink_;
    ISelfRevertTimer& timer_;
    IConnectivityTransitionHooks& hooks_;
    const std::chrono::milliseconds revert_after_;

    mutable std::mutex mtx_;
    ConnectivityMode mode_{ConnectivityMode::kP2pOnly};  // R-BOOT: boot starts in S1
    MigrationId migration_counter_{0};
    MigrationId current_id_{0};
    std::string preview_endpoint_;
    std::string preview_credential_;
};

}  // namespace sst::network
