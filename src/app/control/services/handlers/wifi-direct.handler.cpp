#include "app/control/services/handlers/wifi-direct.handler.hpp"

namespace sst::control {

WifiDirectHandler::WifiDirectHandler(sst::session::ISessionManager& session, IWifiManager& wifi,
                                     IDhcpServer& dhcp, std::uint32_t preview_port,
                                     std::uint32_t download_port)
    : session_(session),
      wifi_(wifi),
      dhcp_(dhcp),
      preview_port_(preview_port),
      download_port_(download_port) {}

auto WifiDirectHandler::HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> {
    return {sst_cam::Command::kStartWifiDirect, sst_cam::Command::kStopWifiDirect};
}

auto WifiDirectHandler::Handle(const sst_cam::Command& cmd) -> sst_cam::CommandResponse {
    if (cmd.payload_case() == sst_cam::Command::kStopWifiDirect) {
        return HandleStop();
    }
    return HandleStart();
}

auto WifiDirectHandler::HandleStart() -> sst_cam::CommandResponse {
    sst_cam::CommandResponse resp;

    auto group = wifi_.StartP2pGroupOwner();
    if (!group) {
        resp.set_status(sst_cam::ResponseStatus::ERROR);
        resp.set_error_message("failed to form WiFi Direct group owner");
        return resp;
    }

    if (!dhcp_.Start(group->group_interface, group->group_owner_ip)) {
        // The group is up but DHCP failed — tear the group back down so we don't
        // leave a half-open state.
        wifi_.Stop();
        resp.set_status(sst_cam::ResponseStatus::ERROR);
        resp.set_error_message("failed to start DHCP on the WiFi Direct group");
        return resp;
    }

    if (!session_.OnWifiReady()) {
        // SM rejected the transition (e.g. no central connected) — don't report
        // a live group + credentials the session can't actually use. Roll back.
        dhcp_.Stop();
        wifi_.Stop();
        resp.set_status(sst_cam::ResponseStatus::ERROR);
        resp.set_error_message("WiFi Direct group up but session not ready to accept it");
        return resp;
    }

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

auto WifiDirectHandler::HandleStop() -> sst_cam::CommandResponse {
    dhcp_.Stop();
    wifi_.Stop();
    session_.OnWifiStopped();

    sst_cam::CommandResponse resp;
    resp.set_status(sst_cam::ResponseStatus::OK);
    return resp;
}

}  // namespace sst::control
