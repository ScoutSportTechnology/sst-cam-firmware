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
    auto HandleSet(const sst_cam::NetworkConfig& proto) -> sst_cam::CommandResponse;
    // Handle ConfirmPreviewReachableCommand (U4): route the app's reachability
    // verdict to the arbiter's epoch-guarded confirm and report the settled mode.
    auto HandleConfirmReachable(const sst_cam::ConfirmPreviewReachableCommand& cmd)
        -> sst_cam::CommandResponse;
    // Build the Set/Get response (current config + live per-interface status).
    // `migration`, when present, carries the P2P->STA migration id / STA preview
    // endpoint / per-session credential this Set began (U9 fields).
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
    std::mutex mtx_;
};

}  // namespace sst::control
