#pragma once

#include <mutex>
#include <vector>

#include "app/control/ports/handler.hpp"
#include "app/control/ports/link-security.hpp"
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
                   sst::config::UplinkData initial, const ILinkSecurity* link_security = nullptr);

    [[nodiscard]] auto HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> override;
    auto Handle(const sst_cam::Command& cmd) -> sst_cam::CommandResponse override;

   private:
    auto HandleSet(const sst_cam::NetworkConfig& proto) -> sst_cam::CommandResponse;
    // Build the response (current config + live per-interface status). `status`
    // sets the ResponseStatus: OK by default, or the caller's error when an
    // enabled interface failed to apply.
    auto BuildResponse(sst_cam::ResponseStatus status) -> sst_cam::CommandResponse;
    auto HandleGet() -> sst_cam::CommandResponse;

    sst::network::UplinkManager& manager_;
    sst::network::IUplinkStore& store_;
    sst::config::UplinkData current_;
    // Optional credential-link security gate (INV6/AE7). Null = no gate (non-BLE
    // wiring). Non-owning: the BLE transport that implements it outlives this.
    const ILinkSecurity* link_security_;
    std::mutex mtx_;
};

}  // namespace sst::control
