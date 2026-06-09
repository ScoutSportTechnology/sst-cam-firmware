#pragma once

#include <string>

#include "app/control/ports/dhcp-server.hpp"

namespace sst::adapters::control {

// dnsmasq-backed DHCP server (hardware-bound — spawns a dnsmasq process bound to
// the WiFi Direct group interface). Assigns the GO static IP and serves a small
// lease range on the 192.168.49.0/24 subnet for the joining phone. Stops the
// process on teardown.
class DnsmasqDhcpServer final : public sst::control::IDhcpServer {
   public:
    DnsmasqDhcpServer() = default;
    ~DnsmasqDhcpServer() override;

    DnsmasqDhcpServer(const DnsmasqDhcpServer&) = delete;
    auto operator=(const DnsmasqDhcpServer&) -> DnsmasqDhcpServer& = delete;
    DnsmasqDhcpServer(DnsmasqDhcpServer&&) = delete;
    auto operator=(DnsmasqDhcpServer&&) -> DnsmasqDhcpServer& = delete;

    auto Start(const std::string& group_interface, const std::string& go_ip) -> bool override;
    auto Stop() -> void override;

   private:
    int pid_{-1};
};

}  // namespace sst::adapters::control
