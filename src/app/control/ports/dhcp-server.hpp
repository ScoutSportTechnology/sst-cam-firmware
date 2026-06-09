#pragma once

#include <string>

namespace sst::control {

// Hands out a DHCP lease to the phone that joins the WiFi Direct group. Bound to
// the group interface only and serving the GO subnet (KTD4). On-device this is a
// dnsmasq process; the static IP + policy routing are deploy-time provisioning.
class IDhcpServer {
   public:
    virtual ~IDhcpServer() = default;

    // Start serving DHCP on `group_interface` with the group-owner IP
    // `go_ip` (e.g. "192.168.49.1"). Returns false if it could not start.
    virtual auto Start(const std::string& group_interface, const std::string& go_ip) -> bool = 0;
    virtual auto Stop() -> void = 0;
};

}  // namespace sst::control
