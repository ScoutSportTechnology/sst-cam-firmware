#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "adapters/control/network/nmcli-command.hpp"

namespace {

using sst::adapters::control::BuildConnectionUpArgv;
using sst::adapters::control::BuildEthDhcpArgv;
using sst::adapters::control::BuildEthStaticArgv;
using sst::adapters::control::BuildWifiManagedArgv;
using sst::adapters::control::BuildWifiStaAddArgv;
using sst::adapters::control::BuildWifiStaDeleteArgv;

auto Contains(const std::vector<std::string>& argv, const std::string& value) -> bool {
    return std::find(argv.begin(), argv.end(), value) != argv.end();
}

// Index of `key` in argv, or -1. Lets a test assert the value that FOLLOWS a key
// (nmcli takes `<key> <value>` pairs).
auto IndexOf(const std::vector<std::string>& argv, const std::string& key) -> int {
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (argv[i] == key) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

TEST(NmcliCommandTest, DhcpSetsAutoAndClearsStatic) {
    const auto argv = BuildEthDhcpArgv("Wired connection 1");
    ASSERT_FALSE(argv.empty());
    EXPECT_EQ(argv.front(), "nmcli");
    EXPECT_TRUE(Contains(argv, "Wired connection 1"));
    const int method = IndexOf(argv, "ipv4.method");
    ASSERT_GE(method, 0);
    EXPECT_EQ(argv.at(static_cast<std::size_t>(method) + 1), "auto");
    // Static fields are blanked so a prior static address isn't left pinned.
    const int addr = IndexOf(argv, "ipv4.addresses");
    ASSERT_GE(addr, 0);
    EXPECT_EQ(argv.at(static_cast<std::size_t>(addr) + 1), "");
}

TEST(NmcliCommandTest, StaticWithGatewayAndDns) {
    sst::config::EthernetUplink cfg;
    cfg.dhcp = false;
    cfg.address = "10.10.1.30/24";
    cfg.gateway = "10.10.1.1";
    cfg.dns = "1.1.1.1";

    const auto argv = BuildEthStaticArgv("eth-con", cfg);
    ASSERT_FALSE(argv.empty());
    const int method = IndexOf(argv, "ipv4.method");
    ASSERT_GE(method, 0);
    EXPECT_EQ(argv.at(static_cast<std::size_t>(method) + 1), "manual");
    const int addr = IndexOf(argv, "ipv4.addresses");
    ASSERT_GE(addr, 0);
    EXPECT_EQ(argv.at(static_cast<std::size_t>(addr) + 1), "10.10.1.30/24");
    EXPECT_TRUE(Contains(argv, "ipv4.gateway"));
    EXPECT_TRUE(Contains(argv, "10.10.1.1"));
    EXPECT_TRUE(Contains(argv, "ipv4.dns"));
    EXPECT_TRUE(Contains(argv, "1.1.1.1"));
}

TEST(NmcliCommandTest, StaticOmitsAbsentGatewayAndDns) {
    sst::config::EthernetUplink cfg;
    cfg.dhcp = false;
    cfg.address = "192.168.1.50/24";  // no gateway/dns

    const auto argv = BuildEthStaticArgv("eth-con", cfg);
    ASSERT_FALSE(argv.empty());
    EXPECT_TRUE(Contains(argv, "192.168.1.50/24"));
    EXPECT_FALSE(Contains(argv, "ipv4.gateway"));
    EXPECT_FALSE(Contains(argv, "ipv4.dns"));
}

TEST(NmcliCommandTest, StaticRejectsAddressWithoutCidr) {
    sst::config::EthernetUplink cfg;
    cfg.dhcp = false;
    cfg.address = "192.168.1.50";  // no prefix length
    EXPECT_TRUE(BuildEthStaticArgv("eth-con", cfg).empty());
}

TEST(NmcliCommandTest, EmptyConnectionRejected) {
    EXPECT_TRUE(BuildEthDhcpArgv("").empty());
    EXPECT_TRUE(BuildConnectionUpArgv("").empty());
    sst::config::EthernetUplink cfg;
    cfg.address = "10.0.0.1/24";
    EXPECT_TRUE(BuildEthStaticArgv("", cfg).empty());
}

TEST(NmcliCommandTest, ConnectionUp) {
    const auto argv = BuildConnectionUpArgv("Wired connection 1");
    ASSERT_EQ(argv.size(), 4U);
    EXPECT_EQ(argv[0], "nmcli");
    EXPECT_EQ(argv[1], "con");
    EXPECT_EQ(argv[2], "up");
    EXPECT_EQ(argv[3], "Wired connection 1");
}

// --- WiFi STA uplink builders (U2) ---

TEST(NmcliCommandTest, WifiManagedHandsIfaceToNm) {
    const auto argv = BuildWifiManagedArgv("wlP1p1s0");
    ASSERT_EQ(argv.size(), 6U);
    EXPECT_EQ(argv[0], "nmcli");
    EXPECT_EQ(argv[1], "device");
    EXPECT_EQ(argv[2], "set");
    EXPECT_EQ(argv[3], "wlP1p1s0");
    EXPECT_EQ(argv[4], "managed");
    EXPECT_EQ(argv[5], "yes");
    EXPECT_TRUE(BuildWifiManagedArgv("").empty());
}

TEST(NmcliCommandTest, WifiStaDeleteRejectsEmptyProfile) {
    EXPECT_FALSE(BuildWifiStaDeleteArgv("sst-uplink-wifi").empty());
    EXPECT_TRUE(BuildWifiStaDeleteArgv("").empty());
}

TEST(NmcliCommandTest, WifiStaAddDhcpWithPsk) {
    sst::config::WifiStaUplink cfg;
    cfg.ssid = "PhoneNet";
    cfg.passphrase = "hunter2pw";
    cfg.dhcp = true;

    const auto argv = BuildWifiStaAddArgv("sst-uplink-wifi", "wlP1p1s0", cfg);
    ASSERT_FALSE(argv.empty());
    EXPECT_TRUE(Contains(argv, "wifi"));
    // ssid follows the "ssid" key.
    const int ssid = IndexOf(argv, "ssid");
    ASSERT_GE(ssid, 0);
    EXPECT_EQ(argv.at(static_cast<std::size_t>(ssid) + 1), "PhoneNet");
    // WPA-PSK carried.
    EXPECT_TRUE(Contains(argv, "wifi-sec.key-mgmt"));
    EXPECT_TRUE(Contains(argv, "wpa-psk"));
    const int psk = IndexOf(argv, "wifi-sec.psk");
    ASSERT_GE(psk, 0);
    EXPECT_EQ(argv.at(static_cast<std::size_t>(psk) + 1), "hunter2pw");
    // DHCP + never auto-join (R-BOOT/INV1).
    const int method = IndexOf(argv, "ipv4.method");
    ASSERT_GE(method, 0);
    EXPECT_EQ(argv.at(static_cast<std::size_t>(method) + 1), "auto");
    const int autoc = IndexOf(argv, "connection.autoconnect");
    ASSERT_GE(autoc, 0);
    EXPECT_EQ(argv.at(static_cast<std::size_t>(autoc) + 1), "no");
}

TEST(NmcliCommandTest, WifiStaAddStaticRequiresCidr) {
    sst::config::WifiStaUplink cfg;
    cfg.ssid = "PhoneNet";
    cfg.passphrase = "pw";
    cfg.dhcp = false;
    cfg.address = "10.0.0.5";  // no prefix length
    EXPECT_TRUE(BuildWifiStaAddArgv("sst-uplink-wifi", "wlP1p1s0", cfg).empty());

    cfg.address = "10.0.0.5/24";
    const auto argv = BuildWifiStaAddArgv("sst-uplink-wifi", "wlP1p1s0", cfg);
    ASSERT_FALSE(argv.empty());
    const int method = IndexOf(argv, "ipv4.method");
    ASSERT_GE(method, 0);
    EXPECT_EQ(argv.at(static_cast<std::size_t>(method) + 1), "manual");
    EXPECT_TRUE(Contains(argv, "10.0.0.5/24"));
}

TEST(NmcliCommandTest, WifiStaAddOpenNetworkOmitsPsk) {
    sst::config::WifiStaUplink cfg;
    cfg.ssid = "OpenCafe";  // no passphrase → open
    const auto argv = BuildWifiStaAddArgv("sst-uplink-wifi", "wlP1p1s0", cfg);
    ASSERT_FALSE(argv.empty());
    EXPECT_FALSE(Contains(argv, "wifi-sec.key-mgmt"));
}

TEST(NmcliCommandTest, WifiStaAddRejectsMissingSsidOrDevice) {
    sst::config::WifiStaUplink cfg;  // no ssid
    EXPECT_TRUE(BuildWifiStaAddArgv("sst-uplink-wifi", "wlP1p1s0", cfg).empty());
    cfg.ssid = "PhoneNet";
    EXPECT_TRUE(BuildWifiStaAddArgv("sst-uplink-wifi", "", cfg).empty());
    EXPECT_TRUE(BuildWifiStaAddArgv("", "wlP1p1s0", cfg).empty());
}

}  // namespace
