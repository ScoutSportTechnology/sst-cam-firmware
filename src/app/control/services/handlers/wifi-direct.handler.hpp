#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "app/control/ports/dhcp-server.hpp"
#include "app/control/ports/handler.hpp"
#include "app/control/ports/network-configurator.hpp"
#include "app/control/ports/wifi-manager.hpp"
#include "app/network/ports/connectivity-arbiter.hpp"
#include "app/session/ports/session-manager.hpp"
#include "app/streaming/ports/streaming-service.hpp"
#include "bluetooth.pb.h"
#include "domain/control/models/service-ports.hpp"
#include "domain/network/models/wifi-direct-group.hpp"
#include "domain/streaming/models/app-stream-config.hpp"

namespace sst::control {

// Handles StartWifiDirect / StopWifiDirect. Start forms a real autonomous P2P
// group owner, brings up DHCP on the group interface, advances the session to
// WifiReady, starts the RTSP preview stream bound to the group-owner IP, and
// reports the camera-generated credentials in a WifiDirectGroupResponse (KTD4).
class WifiDirectHandler final : public ICommandHandler,
                                 public sst::network::IConnectivityTransitionHooks {
   public:
    WifiDirectHandler(sst::session::ISessionManager& session, IWifiManager& wifi,
                      INetworkConfigurator& netcfg, IDhcpServer& dhcp,
                      sst::streaming::IStreamingService& streaming, PreviewPort preview_port,
                      DownloadPort download_port,
                      std::function<sst_cam::CameraHealth(std::uint32_t)> health = {});

    [[nodiscard]] auto HandledCases() const -> std::vector<sst_cam::Command::PayloadCase> override;
    auto Handle(const sst_cam::Command& cmd) -> sst_cam::CommandResponse override;

    // IConnectivityTransitionHooks
    auto OnSharedWifiJoined(const std::string& sta_lan_ip) -> std::string override;
    auto OnRevertedToP2p() -> bool override;

   private:
    auto HandleStart() -> sst_cam::CommandResponse;
    auto HandleStop() -> sst_cam::CommandResponse;

    // Bring the GO radio fully up: form the P2P group, assign the GO address, and
    // start DHCP. Sets group_interface_. Rolls back + returns nullopt on any step
    // failure. Shared by HandleStart and the OnRevertedToP2p restore path.
    auto BringUpGroupOwnerRadio() -> std::optional<sst::network::WifiDirectGroup>;
    // Build the preview AppStreamConfig for `address`; a non-empty `credential`
    // makes the preview require Basic auth (S2/INV5).
    [[nodiscard]] auto MakePreviewConfig(const std::string& address, const std::string& credential)
        const -> sst::streaming::AppStreamConfig;
    // Mint a fresh per-session RTSP credential ("cam:<random>") for S2 (U10).
    static auto MintPreviewCredential() -> std::string;

    sst::session::ISessionManager& session_;
    IWifiManager& wifi_;
    INetworkConfigurator& netcfg_;
    IDhcpServer& dhcp_;
    sst::streaming::IStreamingService& streaming_;
    std::uint32_t preview_port_;
    std::uint32_t download_port_;
    std::function<sst_cam::CameraHealth(std::uint32_t)> health_;
    // Group interface of the active session, retained so HandleStop can flush its
    // address before the group is removed.
    std::string group_interface_;
    std::optional<sst::network::WifiDirectGroup> active_group_;
};

}  // namespace sst::control
