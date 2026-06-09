#pragma once

#include <cstdint>
#include <vector>

#include "app/control/ports/dhcp-server.hpp"
#include "app/control/ports/handler.hpp"
#include "app/control/ports/wifi-manager.hpp"
#include "app/session/ports/session-manager.hpp"
#include "bluetooth.pb.h"

namespace sst::control {

// Handles StartWifiDirect / StopWifiDirect. Start forms a real autonomous P2P
// group owner, brings up DHCP on the group interface, advances the session to
// WifiReady, and reports the camera-generated credentials in a
// WifiDirectGroupResponse (KTD4). Stop tears the group + DHCP down.
class WifiDirectHandler final : public ICommandHandler {
   public:
    WifiDirectHandler(sst::session::ISessionManager& session, IWifiManager& wifi,
                      IDhcpServer& dhcp, std::uint32_t preview_port,
                      std::uint32_t download_port);

    auto HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> override;
    auto Handle(const sst_cam::Command& cmd) -> sst_cam::CommandResponse override;

   private:
    auto HandleStart() -> sst_cam::CommandResponse;
    auto HandleStop() -> sst_cam::CommandResponse;

    sst::session::ISessionManager& session_;
    IWifiManager& wifi_;
    IDhcpServer& dhcp_;
    std::uint32_t preview_port_;
    std::uint32_t download_port_;
};

}  // namespace sst::control
