#include "app/control/services/handlers/network.handler.hpp"

#include <spdlog/spdlog.h>

#include <utility>

namespace sst::control {

namespace {

// proto NetworkConfig (flat scalars) -> domain UplinkData (optionals). proto3
// scalars are always present, so every field becomes an engaged optional —
// SetNetworkConfig is a FULL REPLACE of the uplink config (see uplink-config.hpp).
auto ProtoToUplink(const sst_cam::NetworkConfig& proto) -> sst::config::UplinkData {
    sst::config::UplinkData out;
    const auto& eth = proto.ethernet();
    out.ethernet.enabled = eth.enabled();
    out.ethernet.dhcp = eth.dhcp();
    out.ethernet.address = eth.address();
    out.ethernet.gateway = eth.gateway();
    out.ethernet.dns = eth.dns();
    const auto& wifi = proto.wifi();
    out.wifi.enabled = wifi.enabled();
    out.wifi.ssid = wifi.ssid();
    out.wifi.passphrase = wifi.password();
    out.wifi.dhcp = wifi.dhcp();
    out.wifi.address = wifi.address();
    out.wifi.gateway = wifi.gateway();
    out.wifi.dns = wifi.dns();
    return out;
}

// domain UplinkData -> proto NetworkConfig (unset optional -> proto default).
void UplinkToProto(const sst::config::UplinkData& data, sst_cam::NetworkConfig* proto) {
    auto* eth = proto->mutable_ethernet();
    eth->set_enabled(data.ethernet.enabled.value_or(false));
    eth->set_dhcp(data.ethernet.dhcp.value_or(true));
    eth->set_address(data.ethernet.address.value_or(""));
    eth->set_gateway(data.ethernet.gateway.value_or(""));
    eth->set_dns(data.ethernet.dns.value_or(""));
    auto* wifi = proto->mutable_wifi();
    wifi->set_enabled(data.wifi.enabled.value_or(false));
    wifi->set_ssid(data.wifi.ssid.value_or(""));
    wifi->set_password(data.wifi.passphrase.value_or(""));
    wifi->set_dhcp(data.wifi.dhcp.value_or(true));
    wifi->set_address(data.wifi.address.value_or(""));
    wifi->set_gateway(data.wifi.gateway.value_or(""));
    wifi->set_dns(data.wifi.dns.value_or(""));
}

}  // namespace

NetworkHandler::NetworkHandler(sst::network::UplinkManager& manager,
                               sst::network::IUplinkStore& store, sst::config::UplinkData initial,
                               sst::network::IConnectivityArbiter& arbiter,
                               const ILinkSecurity* link_security)
    : manager_(manager),
      store_(store),
      current_(std::move(initial)),
      arbiter_(arbiter),
      link_security_(link_security) {}

auto NetworkHandler::HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> {
    return {sst_cam::Command::kSetNetworkConfig, sst_cam::Command::kGetNetworkConfig,
            sst_cam::Command::kConfirmPreviewReachable};
}

auto NetworkHandler::Handle(const sst_cam::Command& cmd) -> sst_cam::CommandResponse {
    if (cmd.payload_case() == sst_cam::Command::kSetNetworkConfig) {
        return HandleSet(cmd.set_network_config().config());
    }
    if (cmd.payload_case() == sst_cam::Command::kConfirmPreviewReachable) {
        return HandleConfirmReachable(cmd.confirm_preview_reachable());
    }
    return HandleGet();
}

auto NetworkHandler::HandleSet(const sst_cam::NetworkConfig& proto) -> sst_cam::CommandResponse {
    std::lock_guard lock(mtx_);

    // INV6 / AE7: SetNetworkConfig carries the WiFi passphrase. If a credential-
    // link security gate is wired and the link is NOT encrypted/bonded, reject
    // BEFORE mapping/applying/persisting — the passphrase must never be accepted
    // over cleartext. current_ is left untouched so a rejected write cannot
    // clobber the live config. A null gate = no BLE link-security check (non-BLE
    // wiring / tests).
    if (link_security_ != nullptr && !link_security_->IsCredentialLinkSecure()) {
        spdlog::error(
            "NetworkHandler: SetNetworkConfig rejected — BLE link is not "
            "encrypted/bonded; WiFi credentials refused over cleartext (INV6/AE7)");
        auto resp = BuildResponse(sst_cam::ResponseStatus::ERROR, std::nullopt);
        resp.set_error_message(
            "WiFi credentials require an encrypted, bonded Bluetooth link; pair the "
            "device and try again.");
        return resp;
    }

    current_ = ProtoToUplink(proto);

    // Ethernet is applied directly (it is orthogonal to the radio). The WiFi-STA
    // radio is NOT applied here — it is owned by the connectivity arbiter (INV2:
    // the GO must be torn down before STA comes up), so we mask wifi out of the
    // manager apply and route it through the arbiter below.
    sst::config::UplinkData ethernet_only = current_;
    ethernet_only.wifi.enabled = false;
    const auto applied = manager_.Apply(ethernet_only);
    if (applied.ethernet_enabled && !applied.ethernet.ok) {
        spdlog::error("NetworkHandler: ethernet apply failed ('{}') — not persisting, ERROR",
                      applied.ethernet.detail);
        return BuildResponse(sst_cam::ResponseStatus::ERROR, std::nullopt);
    }

    // Route the WiFi-STA request through the arbiter. Enabled -> begin a P2P->STA
    // migration (GO down, STA up, mint credential, await the app's reachability
    // confirm). Disabled -> ensure we are back in S1.
    std::optional<sst::network::SharedWifiMigration> migration;
    if (current_.wifi.enabled.value_or(false)) {
        migration = arbiter_.RequestSharedWifi(current_.wifi);
        if (!migration) {
            // STA join failed (bad passphrase / isolation) or a migration was
            // already in flight — the arbiter has reverted to a legal S1. Report
            // the failure so the app shows a friendly message (U7) rather than a
            // false green, and do not persist a config that did not come up.
            spdlog::error(
                "NetworkHandler: shared-wifi migration could not start — reporting ERROR");
            return BuildResponse(sst_cam::ResponseStatus::ERROR, std::nullopt);
        }
    } else {
        arbiter_.RevertToP2p();
    }

    // Only durable once the config was accepted. autoconnect=no on the STA profile
    // holds R-BOOT (no self-join on the next boot).
    if (!store_.Persist(current_)) {
        spdlog::error("NetworkHandler: uplink accepted but persistence failed");
        return BuildResponse(sst_cam::ResponseStatus::ERROR, migration);
    }
    return BuildResponse(sst_cam::ResponseStatus::OK, migration);
}

auto NetworkHandler::HandleConfirmReachable(const sst_cam::ConfirmPreviewReachableCommand& cmd)
    -> sst_cam::CommandResponse {
    std::lock_guard lock(mtx_);
    const auto mode = arbiter_.ConfirmSharedWifi(cmd.migration_id(), cmd.reachable());
    sst_cam::CommandResponse resp;
    resp.set_status(sst_cam::ResponseStatus::OK);
    auto* payload = resp.mutable_confirm_preview_reachable();
    payload->set_migration_id(cmd.migration_id());
    payload->set_in_shared_wifi(mode == sst::network::ConnectivityMode::kStaOnly);
    spdlog::info("NetworkHandler: confirm id={} reachable={} -> in_shared_wifi={}",
                 cmd.migration_id(), cmd.reachable(),
                 mode == sst::network::ConnectivityMode::kStaOnly);
    return resp;
}

auto NetworkHandler::BuildResponse(
    sst_cam::ResponseStatus status,
    const std::optional<sst::network::SharedWifiMigration>& migration) -> sst_cam::CommandResponse {
    // Live interface state (reality), not the last-applied result — the camera's
    // ethernet is NM-managed and may be up even when the uplink config disables it.
    const auto live = manager_.QueryStatus();
    sst_cam::CommandResponse resp;
    auto* payload = resp.mutable_network_config();
    UplinkToProto(current_, payload->mutable_config());
    payload->set_ethernet_up(live.ethernet.ok);
    payload->set_ethernet_address(live.ethernet.detail);
    payload->set_wifi_up(live.wifi.ok);
    payload->set_wifi_status(live.wifi.detail);
    // Shared-WiFi migration handshake (U9): populated only when this Set began a
    // P2P->STA migration — the app probes preview_endpoint (with preview_credential)
    // then replies with ConfirmPreviewReachableCommand{migration_id}.
    if (migration.has_value()) {
        payload->set_migration_id(migration->id);
        payload->set_preview_endpoint(migration->preview_endpoint);
        payload->set_preview_credential(migration->preview_credential);
    }
    resp.set_status(status);
    return resp;
}

auto NetworkHandler::HandleGet() -> sst_cam::CommandResponse {
    return BuildResponse(sst_cam::ResponseStatus::OK, std::nullopt);
}

}  // namespace sst::control
