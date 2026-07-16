#pragma once

#include <mutex>
#include <optional>
#include <vector>

#include "app/control/ports/handler.hpp"
#include "app/control/ports/link-security.hpp"
#include "app/network/ports/connectivity-arbiter.hpp"
#include "app/network/ports/ethernet-controller.hpp"
#include "app/network/ports/uplink-store.hpp"
#include "app/network/services/uplink-manager/uplink-manager.hpp"
#include "bluetooth.pb.h"
#include "domain/config/models/uplink-config.hpp"

namespace sst::control {

// Handles Set/GetNetworkConfig (U4 / R3, R8): the BLE surface for the camera's
// internet uplink. Set maps the proto config to the domain UplinkData (a FULL
// REPLACE — see uplink-config.hpp), applies it via the UplinkManager (ethernet
// / gated wifi-STA), and — only when the apply succeeded for every ENABLED
// interface — PERSISTS it via the IUplinkStore so it survives restart. The
// response carries live per-interface status and reports ERROR (not OK) when an
// enabled interface failed to come up, so the app does not see a green/OK for a
// config that is actually down. Get returns the current config + live status
// without re-applying. Persistence goes through a port (IUplinkStore) so this
// app-layer handler does not depend on the JSON serde / filesystem (adapters).
//
// INV6 / AE7 (credential-link security): SetNetworkConfig carries the WiFi
// passphrase. The proto contract rides EVERY command on a single Command-Write
// characteristic, so there is no separate credential characteristic to protect;
// the GATT `encrypt-authenticated-write` flag is the primary gate and this
// handler is the app-layer double-check. When an ILinkSecurity predicate is
// provided and reports the link is NOT encrypted/bonded, SetNetworkConfig is
// rejected (ERROR) BEFORE the passphrase is applied or persisted. A null
// predicate (non-BLE / test wiring) is treated as no-gate. GetNetworkConfig and
// every other command are unaffected — reconnect UX is preserved.
class NetworkHandler final : public ICommandHandler {
   public:
    NetworkHandler(sst::network::UplinkManager& manager, sst::network::IUplinkStore& store,
                   sst::config::UplinkData initial, sst::network::IConnectivityArbiter& arbiter,
                   sst::network::IEthernetController& ethernet,
                   const ILinkSecurity* link_security = nullptr);

    [[nodiscard]] auto HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> override;
    auto Handle(const sst_cam::Command& cmd) -> sst_cam::CommandResponse override;

   private:
    // Route the FULL SetNetworkConfigCommand (config + the phone_subnet_cidr hint)
    // through the arbiter's SINGLE LAN-path decision site (R18) — no direct Ethernet
    // apply bypass.
    auto HandleSet(const sst_cam::SetNetworkConfigCommand& cmd) -> sst_cam::CommandResponse;
    // Handle ConfirmPreviewReachableCommand (STA leg): route the app's reachability
    // verdict to the arbiter's epoch-guarded confirm and report the settled mode.
    auto HandleConfirmReachable(const sst_cam::ConfirmPreviewReachableCommand& cmd)
        -> sst_cam::CommandResponse;
    // Handle ConfirmLanPathCommand (R11/R19): route the app's LAN-path reachability
    // verdict (confirm id + path) to the arbiter and report the committed path.
    auto HandleConfirmLanPath(const sst_cam::ConfirmLanPathCommand& cmd)
        -> sst_cam::CommandResponse;
    // Build the Set/Get response (current config + live per-interface status).
    // `migration`, when present, carries the P2P->STA migration id / STA preview
    // endpoint / per-session credential this Set began (U9 fields).
    //
    // U8 (credential-on-Get): the LAN credential (STA preview OR Ethernet) for the
    // CURRENT committed LAN path is surfaced on BOTH Set and Get — so the app can
    // identity-gate "connected" on reconnect (R8) without replaying a migration. The
    // credential (the secret half) is ALWAYS gated behind the INV6 link-security
    // check here, since HandleGet does not otherwise pass through the HandleSet gate;
    // over an unencrypted/unbonded link only the non-secret endpoint/confirm-id ride.
    // Each committed path carries its OWN freshly-minted credential (Ethernet via
    // OnEthernetLanSelected, STA via OnSharedWifiJoined), so a captured value dies
    // when the path changes — there is no long-lived static credential.
    auto BuildResponse(sst_cam::ResponseStatus status,
                       const std::optional<sst::network::SharedWifiMigration>& migration,
                       const std::string& error_message = {}) -> sst_cam::CommandResponse;
    auto HandleGet() -> sst_cam::CommandResponse;

    sst::network::UplinkManager& manager_;
    sst::network::IUplinkStore& store_;
    sst::config::UplinkData current_;
    // Owns the P2P<->STA radio transition; the WiFi-STA uplink is driven through
    // it (never directly via the manager) so INV2 holds.
    sst::network::IConnectivityArbiter& arbiter_;
    // Firmware-owned Ethernet interface — the source of truth for the REAL
    // static-vs-DHCP mode + address the response reports (R12), replacing the old
    // default-DHCP placeholder.
    sst::network::IEthernetController& ethernet_;
    // Optional credential-link security gate (INV6/AE7). Null = no gate (non-BLE
    // wiring). Non-owning: the BLE transport that implements it outlives this.
    const ILinkSecurity* link_security_;
    // U8 boot-before-Set gap: true once an operator SetNetworkConfig has driven the
    // arbiter's SelectLanPath, after which arbiter_.CurrentLanPath() is the
    // authoritative LAN path (it correctly EXCLUDES a different-subnet Ethernet
    // uplink). Before that, boot-apply (uplink_manager.Apply on a detached thread)
    // may bring a persisted Ethernet/STA up OUTSIDE the arbiter, which still reports
    // its kP2p boot default — so a plain Get derives the LAN path from the live
    // probes to report REALITY. Guarded by mtx_.
    bool arbiter_engaged_{false};
    std::mutex mtx_;
};

}  // namespace sst::control
