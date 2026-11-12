#include "adapters/control/wifi/wpa_supplicant/ip-network-configurator.hpp"

#include <net/if.h>
#include <spdlog/spdlog.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "adapters/control/network/subprocess.hpp"
#include "adapters/control/wifi/wpa_supplicant/ip-command.hpp"

namespace sst::adapters::control {

namespace {
// Carrier-readiness poll for the freshly-created P2P group interface. wpa forms
// the group and we assign the address, but returning before the kernel marks the
// iface UP+RUNNING races the phone's DHCP DISCOVER → intermittent "wifi failed"
// that only clears on a manual retry. ~1s budget (20 × 50ms) is ample for a local
// group iface; if it never comes up the group is genuinely broken.
constexpr int kIfaceReadyAttempts = 20;
constexpr auto kIfaceReadyInterval = std::chrono::milliseconds(50);

auto InterfaceRunning(const std::string& iface) -> bool {
    const int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }
    ifreq ifr{};
    std::strncpy(static_cast<char*>(ifr.ifr_name), iface.c_str(), IFNAMSIZ - 1);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    const bool running = ::ioctl(sock, SIOCGIFFLAGS, &ifr) == 0 && (ifr.ifr_flags & IFF_UP) != 0 &&
                         (ifr.ifr_flags & IFF_RUNNING) != 0;
    ::close(sock);
    return running;
}

auto WaitInterfaceRunning(const std::string& iface) -> bool {
    for (int attempt = 0; attempt < kIfaceReadyAttempts; ++attempt) {
        if (InterfaceRunning(iface)) {
            return true;
        }
        std::this_thread::sleep_for(kIfaceReadyInterval);
    }
    return false;
}

}  // namespace

auto IpNetworkConfigurator::AssignGroupOwnerAddress(const std::string& iface,
                                                    const std::string& cidr) -> bool {
    const auto replace_argv = BuildAddrReplaceArgv(cidr, iface);
    const auto up_argv = BuildLinkUpArgv(iface);
    if (replace_argv.empty() || up_argv.empty()) {
        spdlog::error("IpNetworkConfigurator: invalid iface '{}' / cidr '{}'", iface, cidr);
        return false;
    }
    if (!RunBounded(replace_argv, kQueryTimeout)) {
        spdlog::error("IpNetworkConfigurator: `ip addr replace {} dev {}` failed", cidr, iface);
        return false;
    }
    // The wpa-created group iface is usually already up; the link-up call itself
    // is best-effort (a no-op `up` on an already-up iface can return non-zero).
    // Carrier readiness, not the call's exit code, is the real gate below.
    if (!RunBounded(up_argv, kQueryTimeout)) {
        spdlog::warn("IpNetworkConfigurator: `ip link set {} up` returned non-zero", iface);
    }
    // Block until the kernel marks the iface UP+RUNNING before returning OK, so the
    // handler does not hand the app credentials for a link the phone cannot yet
    // associate/DHCP against. Fatal: the handler tears the group down on false.
    if (!WaitInterfaceRunning(iface)) {
        spdlog::error("IpNetworkConfigurator: '{}' not UP+RUNNING after bring-up; aborting", iface);
        return false;
    }
    spdlog::info("IpNetworkConfigurator: assigned {} to {} (UP+RUNNING)", cidr, iface);
    return true;
}

auto IpNetworkConfigurator::Clear(const std::string& iface) -> void {
    if (iface.empty()) {
        return;
    }
    // Best-effort: the group iface usually disappears at P2P_GROUP_REMOVE; flushing
    // guards against a reused iface name carrying a stale address.
    RunBounded({"ip", "addr", "flush", "dev", iface}, kQueryTimeout);
}

}  // namespace sst::adapters::control
