#include "app/control/services/handlers/wifi-direct.handler.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <random>
#include <utility>

#include "domain/streaming/models/app-stream-config.hpp"

namespace sst::control {

namespace {
// Subnet prefix for the group-owner address. The /24 matches the dnsmasq lease
// range (192.168.49.0/24); group_owner_ip carries only the host address.
constexpr const char* kGoSubnetSuffix = "/24";
}  // namespace

WifiDirectHandler::WifiDirectHandler(sst::session::ISessionManager& session, IWifiManager& wifi,
                                     INetworkConfigurator& netcfg, IDhcpServer& dhcp,
                                     sst::streaming::IStreamingService& streaming,
                                     PreviewPort preview_port, DownloadPort download_port,
                                     std::function<sst_cam::CameraHealth(std::uint32_t)> health)
    : session_(session),
      wifi_(wifi),
      netcfg_(netcfg),
      dhcp_(dhcp),
      streaming_(streaming),
      preview_port_(preview_port.value),
      download_port_(download_port.value),
      health_(std::move(health)) {}

auto WifiDirectHandler::HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> {
    return {sst_cam::Command::kStartWifiDirect, sst_cam::Command::kStopWifiDirect};
}

auto WifiDirectHandler::Handle(const sst_cam::Command& cmd) -> sst_cam::CommandResponse {
    if (cmd.payload_case() == sst_cam::Command::kStopWifiDirect) {
        return HandleStop();
    }
    return HandleStart();
}

auto WifiDirectHandler::BringUpGroupOwnerRadio() -> std::optional<sst::network::WifiDirectGroup> {
    auto group = wifi_.StartP2pGroupOwner();
    if (!group) {
        spdlog::error("WifiDirectHandler: failed to form WiFi Direct group owner");
        return std::nullopt;
    }

    // Assign the group-owner IP to the freshly-created P2P interface BEFORE DHCP
    // and RTSP — wpa_supplicant forms the group but never assigns an address, so
    // without this dnsmasq has no subnet and RTSP cannot bind the GO IP.
    if (!netcfg_.AssignGroupOwnerAddress(group->group_interface,
                                         group->group_owner_ip + kGoSubnetSuffix)) {
        wifi_.Stop();
        spdlog::error("WifiDirectHandler: failed to assign group-owner IP");
        return std::nullopt;
    }
    group_interface_ = group->group_interface;

    if (!dhcp_.Start(group->group_interface, group->group_owner_ip)) {
        // The group is up but DHCP failed — tear it back down so we don't leave a
        // half-open state.
        wifi_.Stop();
        group_interface_.clear();
        spdlog::error("WifiDirectHandler: failed to start DHCP on the WiFi Direct group");
        return std::nullopt;
    }
    return group;
}

auto WifiDirectHandler::MakePreviewConfig(const std::string& address, const std::string& credential)
    const -> sst::streaming::AppStreamConfig {
    sst::streaming::AppStreamConfig cfg;
    cfg.address = address;
    cfg.port = static_cast<std::uint16_t>(preview_port_);
    cfg.auth_credential = credential;
    return cfg;
}

auto WifiDirectHandler::MintPreviewCredential() -> std::string {
    // Per-session RTSP Basic-auth secret (U10). Random 64-bit hex; the username is
    // fixed ("cam") since the app authenticates with exactly this string.
    std::random_device rng;
    std::uniform_int_distribution<std::uint32_t> dist;
    return fmt::format("cam:{:08x}{:08x}", dist(rng), dist(rng));
}

auto WifiDirectHandler::HandleStart() -> sst_cam::CommandResponse {
    sst_cam::CommandResponse resp;

    if (active_group_ && session_.Snapshot().wifi_group_up) {
        resp.set_status(sst_cam::OK);
        auto* out = resp.mutable_wifi_direct_group();
        out->set_ssid(active_group_->ssid);
        out->set_psk(active_group_->psk);
        out->set_group_owner_ip(active_group_->group_owner_ip);
        out->set_preview_port(preview_port_);
        out->set_download_port(download_port_);
        out->set_role(active_group_->role);
        if (!streaming_.IsAppStreamRunning() &&
            (!health_ || (health_(0) == sst_cam::CAMERA_HEALTH_OK &&
                          health_(1) == sst_cam::CAMERA_HEALTH_OK))) {
            streaming_.StartAppStream(MakePreviewConfig(active_group_->group_owner_ip, ""));
        }
        return resp;
    }

    auto group = BringUpGroupOwnerRadio();
    if (!group) {
        resp.set_status(sst_cam::ResponseStatus::ERROR);
        resp.set_error_message("failed to bring up the WiFi Direct group owner");
        return resp;
    }

    if (!session_.OnWifiReady()) {
        // SM rejected the transition (e.g. no central connected) — don't report
        // a live group + credentials the session can't actually use. Roll back.
        dhcp_.Stop();
        wifi_.Stop();
        group_interface_.clear();
        resp.set_status(sst_cam::ResponseStatus::ERROR);
        resp.set_error_message("WiFi Direct group up but session not ready to accept it");
        return resp;
    }

    if ((!health_ ||
         (health_(0) == sst_cam::CAMERA_HEALTH_OK && health_(1) == sst_cam::CAMERA_HEALTH_OK)) &&
        !streaming_.StartAppStream(MakePreviewConfig(group->group_owner_ip, ""))) {
        spdlog::warn("WifiDirectHandler: RTSP preview failed to start on {}:{} (preview degraded)",
                     group->group_owner_ip, preview_port_);
    }

    active_group_ = *group;
    resp.set_status(sst_cam::ResponseStatus::OK);
    auto* out = resp.mutable_wifi_direct_group();
    out->set_ssid(group->ssid);
    out->set_psk(group->psk);
    out->set_group_owner_ip(group->group_owner_ip);
    out->set_preview_port(preview_port_);
    out->set_download_port(download_port_);
    out->set_role(group->role);
    return resp;
}

auto WifiDirectHandler::OnSharedWifiJoined(const std::string& sta_lan_ip) -> std::string {
    // The arbiter has already stopped the GO radio (INV2). Clean up the residual
    // GO plumbing (DHCP + the GO address on the now-gone iface), then mint a
    // per-session credential and switch the preview onto the STA LAN IP with auth
    // (S2/INV5). The authenticated preview must be live here so the app can probe
    // it before confirming (F1 step 5).
    dhcp_.Stop();
    if (!group_interface_.empty()) {
        netcfg_.Clear(group_interface_);
        group_interface_.clear();
    }
    const std::string credential = MintPreviewCredential();
    // Bind the S2 preview to ALL interfaces (0.0.0.0), NOT the specific STA IP: the
    // DHCP address can still be settling when we rebind, and a specific-IP bind
    // races with that (gst_rtsp_server_attach fails). Per-session auth (INV5) —
    // minted here — is the S2 security, not the bind address. The app connects at
    // the reported STA LAN IP.
    if (!streaming_.RebindAppStream(MakePreviewConfig("0.0.0.0", credential))) {
        spdlog::warn(
            "WifiDirectHandler: failed to rebind preview for STA {} (migration will probe "
            "and likely revert)",
            sta_lan_ip);
    }
    spdlog::info("WifiDirectHandler: preview switched to STA LAN {} (authenticated, bound 0.0.0.0)",
                 sta_lan_ip);
    return credential;
}

auto WifiDirectHandler::OnRevertedToP2p() -> bool {
    // Restore the WiFi-Direct GO preview: re-form the group + DHCP, then rebind the
    // preview to the GO IP with NO auth (S1). The session stays WifiReady across a
    // migration (BLE control never dropped), so no session hook here.
    auto group = BringUpGroupOwnerRadio();
    if (!group) {
        spdlog::error("WifiDirectHandler: GO restore failed on revert (preview degraded)");
        return false;
    }
    if (!streaming_.RebindAppStream(MakePreviewConfig(group->group_owner_ip, ""))) {
        spdlog::warn("WifiDirectHandler: failed to rebind preview to GO {} on revert",
                     group->group_owner_ip);
    }
    spdlog::info("WifiDirectHandler: preview restored to WiFi-Direct GO {}", group->group_owner_ip);
    return true;
}

auto WifiDirectHandler::HandleStop() -> sst_cam::CommandResponse {
    if (session_.Phase() == sst::session::SessionPhase::kRecording ||
        session_.Snapshot().is_streaming) {
        sst_cam::CommandResponse refused;
        refused.set_status(sst_cam::ERROR);
        refused.set_error_message("stop recording and streaming before stopping WiFi Direct");
        return refused;
    }
    dhcp_.Stop();
    // Flush the GO address while the group interface still exists (wifi_.Stop()
    // removes the interface next, which would also drop the address).
    if (!group_interface_.empty()) {
        netcfg_.Clear(group_interface_);
        group_interface_.clear();
    }
    wifi_.Stop();
    active_group_.reset();
    session_.OnWifiStopped();

    sst_cam::CommandResponse resp;
    resp.set_status(sst_cam::ResponseStatus::OK);
    return resp;
}

}  // namespace sst::control
