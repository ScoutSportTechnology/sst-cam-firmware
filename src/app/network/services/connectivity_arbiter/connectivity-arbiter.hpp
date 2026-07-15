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
    auto ConfirmSharedWifi(MigrationId migration_id, bool reachable) -> ConnectivityMode override;
    auto RevertToP2p() -> void override;
    [[nodiscard]] auto CurrentMode() const -> ConnectivityMode override;
    [[nodiscard]] auto LastError() const -> std::string override;
    [[nodiscard]] auto CurrentSsid() const -> std::string override;

    // Accessors for the network command handler (U4) to build NetworkConfigResponse.
    [[nodiscard]] auto CurrentMigrationId() const -> MigrationId;
    [[nodiscard]] auto PreviewEndpoint() const -> std::string;
    [[nodiscard]] auto PreviewCredential() const -> std::string;

   private:
    // Returns the radio to the GO and resets migration state. Caller holds `mtx_`.
    auto DoRevertLocked() -> void;
    // Self-revert timer expiry — no-op unless still kMigrating on the same epoch.
    auto OnSelfRevertTimeout(MigrationId migration_id) -> void;
    // Arm the S2 carrier watchdog (reuses `timer_`, which is idle in kStaOnly).
    // Caller holds `mtx_`.
    auto ArmCarrierWatchdogLocked() -> void;
    // Periodic S2 carrier check (D2a). While kStaOnly, probes the STA link; on a
    // sustained loss it auto-reverts to P2P so live preview resumes over the GO
    // even if BLE is also down (removes the single-point app dependency). Runs on
    // the timer thread; probes WITHOUT `mtx_` held (the probe shells wpa_cli).
    auto OnCarrierCheck() -> void;

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
    std::string current_ssid_;    // SSID being joined / connected to (empty in kP2pOnly)
    std::string last_error_;      // reason the last RequestSharedWifi failed (empty on success)
    int carrier_loss_streak_{0};  // consecutive S2 carrier-check failures (D2a watchdog)
};

}  // namespace sst::network
