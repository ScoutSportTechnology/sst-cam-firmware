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

    // Called when the arbiter selects Ethernet as the LAN path at `ethernet_lan_ip`
    // (R11 Ethernet-LAN handshake). Parallel IN SHAPE to OnSharedWifiJoined but the
    // Ethernet path forms NO WiFi-Direct group and never touches the radio (INV2
    // stays radio-only): rebind the authenticated live preview onto the Ethernet IP
    // and return the RTSP credential ("user:pass"; empty = none) so the phone can
    // identity-probe the Ethernet endpoint before it is confirmed.
    //
    // U4 wires the DECISION + endpoint + shared-epoch confirm id; U6 fills the
    // real body (mint the per-path credential + rebind the RTSP preview server onto
    // the Ethernet IP). A U4-only impl returning "" is a correct minimal body (the
    // endpoint + lan_path are still surfaced; the preview simply is not yet
    // re-bound), NOT a stub.
    virtual auto OnEthernetLanSelected(const std::string& ethernet_lan_ip) -> std::string = 0;
};

// The outcome of a single LAN-path arbitration (U4): which path the arbiter
// selected and, when the phone must confirm reachability before the arbiter
// suppresses the losing alternate (R19), the handshake to probe.
struct LanSelectionResult {
    // false = a HARD failure (e.g. an enabled STA join failed) the handler surfaces
    // as ERROR with `error`; true = a legal selection (which may still be P2P).
    bool ok{true};
    std::string error;  // failure reason, empty on success

    sst::network::LanPath path{sst::network::LanPath::kNone};  // the LAN path selected/committing

    // The reachability handshake the phone must confirm before the arbiter finalizes
    // the pick / suppresses the alternate. confirm_id == 0 means no handshake is
    // pending (already committed — e.g. P2P, or an unsolicited same-subnet hotplug
    // that suppressed STA immediately per R2>R19 and is re-probed on the next Get).
    // Non-zero: the app probes `endpoint` (with `credential`) then replies
    // ConfirmLanPath / ConfirmPreviewReachable carrying this id. For the STA path
    // this mirrors SharedWifiMigration; for the Ethernet path these are the
    // ethernet_endpoint / ethernet_credential / ethernet_confirm_id proto fields.
    MigrationId confirm_id{0};
    std::string endpoint;
    std::string credential;
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

    // The SINGLE LAN-path decision site (R3/R18): given the desired uplink config +
    // the phone-reported subnet (a HINT), pick exactly ONE LAN path — Ethernet-on-
    // phone-subnet -> WiFi-STA -> WiFi-Direct P2P — and drive the transition
    // (probe-confirm-before-suppress, R19). Replaces both the handler's removed
    // direct Ethernet apply and its direct RequestSharedWifi call. The returned
    // handshake, when present, is what the app probes then confirms via
    // ConfirmLanPath / ConfirmPreviewReachable. `ok == false` => an enabled
    // interface failed to come up (handler reports ERROR); the arbiter has already
    // reverted to a legal state.
    virtual auto SelectLanPath(const sst::config::UplinkData& desired,
                               const std::string& phone_subnet_cidr) -> LanSelectionResult = 0;

    // The app's reachability verdict for a migration epoch (KTD5). Only honoured
    // when `id` is the in-flight migration and the arbiter is still kMigrating:
    // reachable=true disarms the timer and finalises kStaOnly; reachable=false
    // reverts to kP2pOnly. A stale/superseded id is a no-op. Returns the resulting
    // mode. (The STA leg of the LAN-path handshake; ConfirmLanPath generalises it.)
    virtual auto ConfirmSharedWifi(MigrationId migration_id,
                                   bool reachable) -> ConnectivityMode = 0;

    // The app's reachability verdict for a LAN-path selection (R19), generalising
    // ConfirmSharedWifi to any path. Finalises the pick ONLY when BOTH
    // `confirm_id == the live epoch` AND `path == the committed selection` (the
    // single-counter epoch guard, KTD): a verdict whose id OR path is stale is a
    // no-op. reachable=true commits `path` and suppresses the losing alternate;
    // reachable=false de-selects and the arbiter falls to the next provider
    // (Ethernet -> STA -> P2P). Returns the LAN path now committed.
    virtual auto ConfirmLanPath(MigrationId confirm_id, sst::network::LanPath path,
                                bool reachable) -> sst::network::LanPath = 0;

    // Force the radio back to P2P-only (STA drop / cleared config / app deny).
    // No-op when already kP2pOnly.
    virtual auto RevertToP2p() -> void = 0;

    [[nodiscard]] virtual auto CurrentMode() const -> ConnectivityMode = 0;

    // The LAN path currently committed / committing (R3). Orthogonal to CurrentMode
    // (radio-only). Default so lightweight test doubles need not implement it.
    [[nodiscard]] virtual auto CurrentLanPath() const -> sst::network::LanPath {
        return sst::network::LanPath::kNone;
    }

    // The Ethernet-LAN handshake to surface in NetworkConfigResponse while Ethernet
    // is the committing/committed LAN path (proto ethernet_endpoint / _credential /
    // _confirm_id). confirm-id 0 / empty strings => no Ethernet handshake pending.
    // U6 fills the credential + preview rebind; U4 populates endpoint + confirm id.
    // Defaults so lightweight doubles need not implement them.
    [[nodiscard]] virtual auto EthernetEndpoint() const -> std::string { return {}; }
    [[nodiscard]] virtual auto EthernetCredential() const -> std::string { return {}; }
    [[nodiscard]] virtual auto EthernetConfirmId() const -> MigrationId { return 0; }

    // A human-readable reason the most recent RequestSharedWifi failed (empty
    // when the last request succeeded or none has run). Lets the command handler
    // put a real reason into CommandResponse.error_message instead of a bare
    // ERROR, so the app can show why a join failed without a debugger. Default
    // returns empty so lightweight test doubles need not implement it.
    [[nodiscard]] virtual auto LastError() const -> std::string { return {}; }

    // The SSID the camera is connected to while in kStaOnly (empty otherwise, and
    // while migrating/reverting). Lets the command handler render "connected to
    // <ssid>" sourced from the arbiter's SETTLED state — never the handler's
    // in-flight config, which a failed change-networks would leave stale. Default
    // empty so lightweight test doubles need not implement it.
    [[nodiscard]] virtual auto CurrentSsid() const -> std::string { return {}; }
};

}  // namespace sst::network
