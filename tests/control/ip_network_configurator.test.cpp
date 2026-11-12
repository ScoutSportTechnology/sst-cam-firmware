// IpNetworkConfigurator guard paths (U2). The successful assign forks `ip` and
// needs CAP_NET_ADMIN + a live interface — that is verified on-device, not here.
// These cover the input-validation guards that reject before any exec.

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "adapters/control/wifi/wpa_supplicant/ip-network-configurator.hpp"

namespace {

using sst::adapters::control::IpNetworkConfigurator;

TEST(IpNetworkConfiguratorTest, RejectsEmptyInterfaceWithoutExec) {
    IpNetworkConfigurator cfg;
    EXPECT_FALSE(cfg.AssignGroupOwnerAddress("", "192.168.49.1/24"));
}

TEST(IpNetworkConfiguratorTest, RejectsCidrWithoutPrefixWithoutExec) {
    IpNetworkConfigurator cfg;
    EXPECT_FALSE(cfg.AssignGroupOwnerAddress("p2p-wlP1p1s0-0", "192.168.49.1"));
}

TEST(IpNetworkConfiguratorTest, ClearReturnsWhenIpHangs) {
    char directory[] = "/tmp/sst-ip-XXXXXX";
    ASSERT_NE(mkdtemp(directory), nullptr);
    const std::filesystem::path executable = std::filesystem::path(directory) / "ip";
    {
        std::ofstream script(executable);
        script << "#!/bin/sh\nexec sleep 30\n";
    }
    ASSERT_EQ(chmod(executable.c_str(), 0700), 0);
    const std::string old_path = std::getenv("PATH") != nullptr ? std::getenv("PATH") : "";
    const std::string path = std::string(directory) + ":" + old_path;
    ASSERT_EQ(setenv("PATH", path.c_str(), 1), 0);
    const auto start = std::chrono::steady_clock::now();
    IpNetworkConfigurator cfg;
    cfg.Clear("p2p-test0");
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(10));
    EXPECT_EQ(setenv("PATH", old_path.c_str(), 1), 0);
    std::filesystem::remove_all(directory);
}

TEST(IpNetworkConfiguratorTest, ClearOnEmptyInterfaceIsNoOp) {
    IpNetworkConfigurator cfg;
    EXPECT_NO_THROW(cfg.Clear(""));
}

}  // namespace
