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

// domain LanPath -> proto LanPath (proto v5). Orthogonal to the radio mode.
auto ToProtoLanPath(sst::network::LanPath path) -> sst_cam::LanPath {
    switch (path) {
        case sst::network::LanPath::kEthernet:
            return sst_cam::LAN_PATH_ETHERNET;
        case sst::network::LanPath::kWifiSta:
            return sst_cam::LAN_PATH_WIFI_STA;
        case sst::network::LanPath::kP2p:
            return sst_cam::LAN_PATH_P2P;
        case sst::network::LanPath::kNone:
            return sst_cam::LAN_PATH_NONE;
    }
    return sst_cam::LAN_PATH_NONE;
}

// proto LanPath -> domain LanPath (for the ConfirmLanPath verdict).
auto FromProtoLanPath(sst_cam::LanPath path) -> sst::network::LanPath {
    switch (path) {
        case sst_cam::LAN_PATH_ETHERNET:
            return sst::network::LanPath::kEthernet;
        case sst_cam::LAN_PATH_WIFI_STA:
            return sst::network::LanPath::kWifiSta;
        case sst_cam::LAN_PATH_P2P:
            return sst::network::LanPath::kP2p;
        default:
            return sst::network::LanPath::kNone;
    }
}

}  // namespace

NetworkHandler::NetworkHandler(sst::network::UplinkManager& manager,
                               sst::network::IUplinkStore& store, sst::config::UplinkData initial,
                               sst::network::IConnectivityArbiter& arbiter,
                               sst::network::IEthernetController& ethernet,
                               const ILinkSecurity* link_security)
    : manager_(manager),
      store_(store),
      current_(std::move(initial)),
      arbiter_(arbiter),
      ethernet_(ethernet),
      link_security_(link_security) {}

auto NetworkHandler::HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> {
    return {sst_cam::Command::kSetNetworkConfig, sst_cam::Command::kGetNetworkConfig,
            sst_cam::Command::kConfirmPreviewReachable, sst_cam::Command::kConfirmLanPath};
}

auto NetworkHandler::Handle(const sst_cam::Command& cmd) -> sst_cam::CommandResponse {
    if (cmd.payload_case() == sst_cam::Command::kSetNetworkConfig) {
        return HandleSet(cmd.set_network_config());
    }
    if (cmd.payload_case() == sst_cam::Command::kConfirmPreviewReachable) {
        return HandleConfirmReachable(cmd.confirm_preview_reachable());
    }
    if (cmd.payload_case() == sst_cam::Command::kConfirmLanPath) {
        return HandleConfirmLanPath(cmd.confirm_lan_path());
    }
    return HandleGet();
}

auto NetworkHandler::HandleSet(const sst_cam::SetNetworkConfigCommand& cmd)
    -> sst_cam::CommandResponse {
    std::lock_guard lock(mtx_);
    const sst_cam::NetworkConfig& proto = cmd.config();

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

    const sst::config::UplinkData incoming = ProtoToUplink(proto);

    // R3 already-connected short-circuit — BEFORE the arbiter, so an operator who
    // re-taps Connect on the network they are ALREADY on gets a clean "connected"
    // reply instead of the re-entrancy guard's "a network change is already in
    // progress". A no-op requires the arbiter to be settled in S2 (kStaOnly) on the
    // SAME SSID *and* the SAME passphrase: same SSID with a DIFFERENT passphrase is
    // NOT a no-op — it must fall through and re-migrate so the arbiter validates the
    // new creds before we persist them (otherwise saved-but-never-applied creds
    // poison the next boot). On a true no-op we touch neither the radio nor the
    // store; the connected state rides back on in_shared_wifi/current_ssid.
    if (incoming.wifi.enabled.value_or(false) &&
        arbiter_.CurrentMode() == sst::network::ConnectivityMode::kStaOnly &&
        arbiter_.CurrentSsid() == incoming.wifi.ssid.value_or("") &&
        current_.wifi.passphrase.value_or("") == incoming.wifi.passphrase.value_or("")) {
        spdlog::info("NetworkHandler: SetNetworkConfig — already connected to '{}' (no-op)",
                     arbiter_.CurrentSsid());
        return BuildResponse(sst_cam::ResponseStatus::OK, std::nullopt);
    }

    // Commit the incoming config, but remember the previous one so a failed
    // selection can roll it back — current_ must never be left showing a network we
    // did not actually bring up (a stale current_ would echo the wrong SSID/creds).
    const sst::config::UplinkData previous = current_;
    current_ = incoming;

    // SINGLE DECISION SITE (R18): route BOTH Ethernet AND WiFi-STA through the
    // arbiter — the handler no longer applies Ethernet directly (the removed
    // manager_.Apply(ethernet_only) bypass). The arbiter picks exactly ONE LAN path
    // (Ethernet-on-phone-subnet -> STA -> P2P) with probe-confirm-before-suppress
    // and drives the transition. phone_subnet_cidr is a HINT (empty when the phone
    // is not on any Wi-Fi); the arbiter reconciles it against ground truth.
    const std::string phone_subnet = cmd.has_phone_subnet_cidr() ? cmd.phone_subnet_cidr() : "";
    const auto selection = arbiter_.SelectLanPath(current_, phone_subnet);
    if (!selection.ok) {
        // An enabled interface failed to come up; the arbiter has reverted to a
        // legal state. Roll current_ back (so the echoed config + displayed
        // connected-state do not point at a network we failed to reach) and report
        // the reason — no false green, and do not persist a config that did not take.
        const std::string reason = selection.error.empty() ? arbiter_.LastError() : selection.error;
        spdlog::error("NetworkHandler: LAN-path selection failed ('{}') — ERROR", reason);
        current_ = previous;
        return BuildResponse(sst_cam::ResponseStatus::ERROR, std::nullopt,
                             reason.empty() ? "Couldn't set up that network" : reason);
    }

    // The STA migration handshake, populated only when THIS Set began a P2P->STA
    // migration (the app probes preview_endpoint then replies with
    // ConfirmPreviewReachableCommand). The Ethernet-LAN handshake rides the
    // lan_path / ethernet_* fields, sourced from the arbiter in BuildResponse.
    std::optional<sst::network::SharedWifiMigration> migration;
    if (selection.path == sst::network::LanPath::kWifiSta && selection.confirm_id != 0) {
        migration = sst::network::SharedWifiMigration{.id = selection.confirm_id,
                                                      .preview_endpoint = selection.endpoint,
                                                      .preview_credential = selection.credential};
    }

    // The Ethernet-LAN handshake credential (R11) rides ONLY the Set that selected
    // Ethernet — surfaced from the arbiter in BuildResponse, gated to this Set so a
    // plain Get never leaks it (U8 broadens credential-on-Get). Mirrors the STA
    // migration credential, which is likewise only carried on a Set reply.
    const bool ethernet_handshake = selection.path == sst::network::LanPath::kEthernet;

    // Only durable once the config was accepted. autoconnect=no on the STA profile
    // holds R-BOOT (no self-join on the next boot).
    if (!store_.Persist(current_)) {
        spdlog::error("NetworkHandler: uplink accepted but persistence failed");
        return BuildResponse(sst_cam::ResponseStatus::ERROR, migration,
                             "Network applied but could not be saved on the camera",
                             ethernet_handshake);
    }
    return BuildResponse(sst_cam::ResponseStatus::OK, migration, {}, ethernet_handshake);
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

auto NetworkHandler::HandleConfirmLanPath(const sst_cam::ConfirmLanPathCommand& cmd)
    -> sst_cam::CommandResponse {
    std::lock_guard lock(mtx_);
    const auto committed =
        arbiter_.ConfirmLanPath(cmd.confirm_id(), FromProtoLanPath(cmd.path()), cmd.reachable());
    sst_cam::CommandResponse resp;
    resp.set_status(sst_cam::ResponseStatus::OK);
    auto* payload = resp.mutable_confirm_lan_path();
    payload->set_confirm_id(cmd.confirm_id());
    payload->set_committed_path(ToProtoLanPath(committed));
    payload->set_committed(committed == FromProtoLanPath(cmd.path()) && cmd.reachable());
    spdlog::info("NetworkHandler: confirm-lan-path id={} path={} reachable={} -> committed={}",
                 cmd.confirm_id(), static_cast<int>(cmd.path()), cmd.reachable(),
                 static_cast<int>(committed));
    return resp;
}

auto NetworkHandler::BuildResponse(
    sst_cam::ResponseStatus status,
    const std::optional<sst::network::SharedWifiMigration>& migration,
    const std::string& error_message,
    bool surface_ethernet_credential) -> sst_cam::CommandResponse {
    // Live interface state (reality), not the last-applied result.
    const auto live = manager_.QueryStatus();
    // Ethernet is sourced from the firmware-owned controller's REAL probe (the
    // firmware owns the config, so it reports the mode it actually set) — NOT the
    // old default-DHCP placeholder that reported "DHCP active" for a static link
    // (the DHCP-when-static bug, R12).
    const auto eth = ethernet_.ProbeEthernet();
    const bool ethernet_up = eth.mode != sst::network::EthernetMode::kDown;
    sst_cam::CommandResponse resp;
    auto* payload = resp.mutable_network_config();
    UplinkToProto(current_, payload->mutable_config());
    payload->set_ethernet_up(ethernet_up);
    payload->set_ethernet_address(eth.address);
    // ethernet_dhcp (proto field 12) is PRESENT only when Ethernet is up: true for
    // DHCP, false for static. Left absent when down — "unknown", never a defaulted
    // DHCP. This is the field the app uses to render the real addressing mode.
    if (ethernet_up) {
        payload->set_ethernet_dhcp(eth.mode == sst::network::EthernetMode::kDhcp);
    }
    payload->set_wifi_up(live.wifi.ok);
    payload->set_wifi_status(live.wifi.detail);
    // Ambient connected-state (R3): sourced from the arbiter's SETTLED mode — never
    // the handler's in-flight current_, which a failed change-networks would leave
    // stale. Read the SSID first, then the mode, and only emit the SSID when the
    // arbiter is confirmed kStaOnly so a concurrent self-revert (timer thread) can
    // only ever downgrade to "not connected", never surface a stale SSID.
    const std::string arbiter_ssid = arbiter_.CurrentSsid();
    const bool in_shared_wifi = arbiter_.CurrentMode() == sst::network::ConnectivityMode::kStaOnly;
    payload->set_in_shared_wifi(in_shared_wifi);
    payload->set_current_ssid(in_shared_wifi ? arbiter_ssid : "");
    // Shared-WiFi migration handshake (U9): populated only when this Set began a
    // P2P->STA migration — the app probes preview_endpoint (with preview_credential)
    // then replies with ConfirmPreviewReachableCommand{migration_id}.
    if (migration.has_value()) {
        payload->set_migration_id(migration->id);
        payload->set_preview_endpoint(migration->preview_endpoint);
        payload->set_preview_credential(migration->preview_credential);
    }

    // Single LAN-path selection (R3, proto v5): the committed/committing path,
    // sourced from the arbiter's settled state — reported on BOTH Set and Get so the
    // app reflects the winning interface. Orthogonal to in_shared_wifi (radio-only).
    const auto lan_path = arbiter_.CurrentLanPath();
    payload->set_lan_path(ToProtoLanPath(lan_path));
    // Ethernet-LAN handshake (R11): the endpoint + confirm id are surfaced whenever
    // Ethernet is the committing/committed LAN path — on BOTH the Set reply that
    // selected it AND a plain Get on reconnect / after an unsolicited hotplug — so the
    // app can identify the live handshake and reply with
    // ConfirmLanPathCommand{ethernet_confirm_id}. Distinct from the STA migration
    // handshake but sharing the same global epoch.
    if (lan_path == sst::network::LanPath::kEthernet) {
        payload->set_ethernet_endpoint(arbiter_.EthernetEndpoint());
        payload->set_ethernet_confirm_id(arbiter_.EthernetConfirmId());
        // The credential (the secret half) is exposed ONLY on the Set that just
        // selected Ethernet (surface_ethernet_credential) — never a plain Get.
        // Reaching here on that path means HandleSet already passed the INV6
        // link-security gate, so the Ethernet credential inherits the SAME
        // encrypted/bonded exposure guard as the STA preview_credential. U8 broadens
        // credential-on-Get with a fresh per-committed-path credential.
        if (surface_ethernet_credential) {
            payload->set_ethernet_credential(arbiter_.EthernetCredential());
        }
    }
    resp.set_status(status);
    // A concrete failure reason for the app to show (Details / friendly copy)
    // instead of a bare ERROR that surfaces as "null". Empty on success.
    resp.set_error_message(error_message);
    return resp;
}

auto NetworkHandler::HandleGet() -> sst_cam::CommandResponse {
    return BuildResponse(sst_cam::ResponseStatus::OK, std::nullopt);
}

}  // namespace sst::control
