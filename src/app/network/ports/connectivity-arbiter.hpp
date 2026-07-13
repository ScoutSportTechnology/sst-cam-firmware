#pragma once

#include <optional>
#include <string>

#include "domain/config/models/uplink-config.hpp"
#include "domain/network/models/connectivity-mode.hpp"

namespace sst::network {

// Side-effects the arbiter drives on a mode transition, kept behind a seam so the
// arbiter owns radio mutual exclusion (INV2) without depending on the streaming /
// WiFi-Direct lifecycle concretely. The concrete impl is wired by U3/U10 (preview
// transport + per-session RTSP credential) and the WiFi-Direct GO choreography.
class IConnectivityTransitionHooks {
   public:
    IConnectivityTransitionHooks() = default;
    virtual ~IConnectivityTransitionHooks() = default;

    IConnectivityTransitionHooks(const IConnectivityTransitionHooks&) = delete;
    auto operator=(const IConnectivityTransitionHooks&) -> IConnectivityTransitionHooks& = delete;
    IConnectivityTransitionHooks(IConnectivityTransitionHooks&&) = delete;
    auto operator=(IConnectivityTransitionHooks&&) -> IConnectivityTransitionHooks& = delete;

    // Called after the radio has joined the phone's network as STA at
    // `sta_lan_ip` (entering kMigrating). Switch the live preview transport to the
    // LAN IP with per-session authentication and return the RTSP credential
    // ("user:pass"); an empty string means no credential was minted. The
    // authenticated preview must be live from here so the app can probe it before
    // confirming (F1 step 5).
    virtual auto OnSharedWifiJoined(const std::string& sta_lan_ip) -> std::string = 0;

    // Called after the radio has been returned to the WiFi-Direct GO (entering
    // kP2pOnly). Restore the GO preview (re-form group, DHCP, unauthenticated RTSP
    // on the GO IP). Return false if it could not be restored — the arbiter still
    // reports kP2pOnly (the radio is no longer STA), but the caller has logged a
    // degraded preview.
    virtual auto OnRevertedToP2p() -> bool = 0;
};

// The result of a successful P2P->STA migration request: what the app needs to
// probe the STA preview and confirm the migration.
struct SharedWifiMigration {
    MigrationId id{0};
    std::string preview_endpoint;    // STA LAN IP now serving the authenticated preview
    std::string preview_credential;  // per-session RTSP "user:pass"; empty = none
};

// The single owner of the camera's WiFi mode (KTD3). It is the only object that
// sequences GO-teardown before STA-up, so INV2 (never STA + P2P concurrently)
// holds by construction — including when re-entrant events arrive mid-migration.
class IConnectivityArbiter {
   public:
    IConnectivityArbiter() = default;
    virtual ~IConnectivityArbiter() = default;

    IConnectivityArbiter(const IConnectivityArbiter&) = delete;
    auto operator=(const IConnectivityArbiter&) -> IConnectivityArbiter& = delete;
    IConnectivityArbiter(IConnectivityArbiter&&) = delete;
    auto operator=(IConnectivityArbiter&&) -> IConnectivityArbiter& = delete;

    // Begin migrating from P2P to the phone's shared WiFi: tear the GO down, join
    // STA, mint the preview credential, arm the self-revert timer, and enter
    // kMigrating. Returns the migration descriptor, or nullopt when a migration is
    // already in flight (re-entrancy guard) or the STA join failed — in the
    // failure case the arbiter has already reverted to a legal kP2pOnly state.
    virtual auto RequestSharedWifi(const sst::config::WifiStaUplink& uplink)
        -> std::optional<SharedWifiMigration> = 0;

    // The app's reachability verdict for a migration epoch (KTD5). Only honoured
    // when `id` is the in-flight migration and the arbiter is still kMigrating:
    // reachable=true disarms the timer and finalises kStaOnly; reachable=false
    // reverts to kP2pOnly. A stale/superseded id is a no-op. Returns the resulting
    // mode.
    virtual auto ConfirmSharedWifi(MigrationId migration_id,
                                   bool reachable) -> ConnectivityMode = 0;

    // Force the radio back to P2P-only (STA drop / cleared config / app deny).
    // No-op when already kP2pOnly.
    virtual auto RevertToP2p() -> void = 0;

    [[nodiscard]] virtual auto CurrentMode() const -> ConnectivityMode = 0;
};

}  // namespace sst::network
