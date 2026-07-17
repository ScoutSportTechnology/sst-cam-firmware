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
        // The arbiter is already committed (kStaOnly from a prior Set) — its
        // CurrentLanPath() is authoritative, so a plain Get must never derive.
        arbiter_engaged_ = true;
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
    // From here on the arbiter has classified the operator's intent, so its
    // CurrentLanPath() is authoritative (even a failed selection reverted to a legal
    // state it now owns) — a plain Get must trust it, never derive from a raw probe.
    arbiter_engaged_ = true;
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

    // Only durable once the config was accepted. autoconnect=no on the STA profile
    // holds R-BOOT (no self-join on the next boot).
    if (!store_.Persist(current_)) {
        spdlog::error("NetworkHandler: uplink accepted but persistence failed");
        return BuildResponse(sst_cam::ResponseStatus::ERROR, migration,
                             "Network applied but could not be saved on the camera");
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
    const std::string& error_message) -> sst_cam::CommandResponse {
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
    // Single LAN-path selection (R3, proto v5): the COMMITTED/committing path from the
    // arbiter's settled state — authoritative, and it already EXCLUDES a
    // different-subnet Ethernet UPLINK (U7 keeps them orthogonal). Drives the LAN
    // handshake below.
    const auto committed_path = arbiter_.CurrentLanPath();

    // Boot-before-Set gap (KNOWN GAP flagged by U4): the arbiter boots kP2p and is
    // only driven to a real LAN path by an operator Set (R-BOOT forbids the arbiter
    // auto-applying a stored uplink), yet boot-apply (uplink_manager.Apply on a
    // detached thread) brings a PERSISTED Ethernet/STA up OUTSIDE the arbiter. So
    // before the first Set has engaged the arbiter, derive the REPORTED lan_path from
    // the live probes so a plain Get reports REALITY (R12). This can NEVER mislabel a
    // different-subnet uplink as the LAN: an uplink only becomes one once a Set feeds
    // the arbiter a phone subnet (it then applies policy routing and commits kP2p /
    // kWifiSta) — from that point arbiter_engaged_ holds and CurrentLanPath() is
    // authoritative, so we never derive. Before any phone subnet is known, "Ethernet
    // is the LAN" matches the arbiter's own empty-phone-subnet classification.
    auto reported_path = committed_path;
    if (!arbiter_engaged_ && (committed_path == sst::network::LanPath::kP2p ||
                              committed_path == sst::network::LanPath::kNone)) {
        if (ethernet_up) {
            reported_path = sst::network::LanPath::kEthernet;
        } else if (live.wifi.ok) {
            reported_path = sst::network::LanPath::kWifiSta;
        }
    }
    payload->set_lan_path(ToProtoLanPath(reported_path));

    // Credential-exposure gate (INV6, U8): the LAN credential (STA preview OR
    // Ethernet) now transits on essentially EVERY Get, so its confidentiality rests
    // entirely on the encrypted/bonded BLE link. HandleGet does NOT pass through the
    // HandleSet INV6 gate, so apply it HERE: surface the SECRET half only when the
    // link is encrypted/bonded. A null gate = non-BLE / test wiring = no-gate (as in
    // HandleSet). The endpoint / confirm-id (non-secret) always ride so the app can
    // address its probe. The credential handed out is the arbiter's LIVE
    // per-committed-path value (Ethernet minted per Ethernet commit, STA per
    // migration; each cleared when the path changes), so a captured value dies with
    // the path — no long-lived static credential.
    const bool credential_link_secure =
        link_security_ == nullptr || link_security_->IsCredentialLinkSecure();

    if (committed_path == sst::network::LanPath::kWifiSta) {
        // STA LAN handshake. migration_id rides ONLY the Set that STARTED the P2P->STA
        // migration (the app replies ConfirmPreviewReachableCommand{migration_id}); on
        // a plain Get of a settled kStaOnly LAN there is no in-flight migration, so the
        // app just re-probes the endpoint to identity-gate "connected" (R8) — no Set.
        if (migration.has_value()) {
            payload->set_migration_id(migration->id);
            payload->set_preview_endpoint(migration->preview_endpoint);
            if (credential_link_secure) {
                payload->set_preview_credential(migration->preview_credential);
            }
        } else {
            // F5: an STA migration in flight whose handshake was NOT carried on a Set
            // reply — the R19 Ethernet->STA fallback: ConfirmLanPath(kEthernet, false)
            // drops Ethernet and mints a FRESH STA epoch + arms the self-revert timer,
            // but that SharedWifiMigration is not returned by the confirm path — must
            // still reach the app so it can ConfirmPreviewReachable{migration_id} before
            // the self-revert timer collapses STA straight to P2P. Surface the live
            // migration id on a plain Get whenever the arbiter is kMigrating; a settled
            // kStaOnly LAN has no in-flight migration (id 0) and the app just re-probes
            // the endpoint to identity-gate "connected" (R8).
            if (arbiter_.CurrentMode() == sst::network::ConnectivityMode::kMigrating) {
                payload->set_migration_id(arbiter_.CurrentMigrationId());
            }
            payload->set_preview_endpoint(arbiter_.PreviewEndpoint());
            if (credential_link_secure) {
                payload->set_preview_credential(arbiter_.PreviewCredential());
            }
        }
    } else if (committed_path == sst::network::LanPath::kEthernet) {
        // Ethernet-LAN handshake (R11): endpoint + confirm id whenever Ethernet is the
        // committed LAN path — on BOTH the Set reply that selected it AND a plain Get
        // on reconnect / after an unsolicited hotplug — so the app can reply
        // ConfirmLanPathCommand{ethernet_confirm_id}. Distinct from the STA migration
        // handshake but sharing the same global epoch. U8 broadens the credential from
        // the Set-only exposure to a plain Get, behind the same link-security gate.
        payload->set_ethernet_endpoint(arbiter_.EthernetEndpoint());
        payload->set_ethernet_confirm_id(arbiter_.EthernetConfirmId());
        if (credential_link_secure) {
            payload->set_ethernet_credential(arbiter_.EthernetCredential());
        }
    }
    // committed_path kP2p / kNone (including a boot-DERIVED reported_path with no
    // committed handshake): no LAN credential is surfaced — there is nothing to hand
    // out until the arbiter commits a path.
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
