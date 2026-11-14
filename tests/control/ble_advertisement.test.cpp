// BLE GATT contract conformance (U5, R2/R2a/R8).
//
// The name-formatting + UUID-constant checks are pure and run in-container.
// The live-advertising check is hardware-bound (real BlueZ over system D-Bus)
// and is expected to FAIL in the cross-compile container — it passes on-device.

#include <gtest/gtest.h>
#include <sdbus-c++/sdbus-c++.h>

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "adapters/control/ble/bluez/bluez-ble-transport.hpp"
#include "domain/control/models/bootstrap-config.hpp"
#include "domain/control/utils/advertised-name.hpp"

namespace {

// R2a: unit number -> zero-padded sst-cam-NNNN.
TEST(BleAdvertisementTest, AdvertisedNameZeroPadsToFourDigits) {
    EXPECT_EQ(sst::control::MakeAdvertisedName(1), "sst-cam-0001");
    EXPECT_EQ(sst::control::MakeAdvertisedName(42), "sst-cam-0042");
    EXPECT_EQ(sst::control::MakeAdvertisedName(9999), "sst-cam-9999");
    EXPECT_EQ(sst::control::MakeAdvertisedName(0), "sst-cam-0000");
    // Beyond 4 digits wraps so the pattern stays fixed-width.
    EXPECT_EQ(sst::control::MakeAdvertisedName(10042), "sst-cam-0042");
}

// Unit number is derived from the device serial number's numeric value.
TEST(BleAdvertisementTest, UnitNumberDerivedFromSerial) {
    EXPECT_EQ(sst::control::DeriveUnitNumber("00000001"), 1U);
    EXPECT_EQ(sst::control::DeriveUnitNumber("00000042"), 42U);
    EXPECT_EQ(sst::control::DeriveUnitNumber("SN-7"), 7U);
    EXPECT_EQ(sst::control::DeriveUnitNumber(""), 0U);
    EXPECT_EQ(sst::control::DeriveUnitNumber("no-digits"), 0U);

    // End-to-end: serial "00000042" advertises as sst-cam-0042.
    EXPECT_EQ(sst::control::MakeAdvertisedName(sst::control::DeriveUnitNumber("00000042")),
              "sst-cam-0042");
}

// R2: the configured GATT UUIDs equal the contract constants
// (proto/README.md "GATT Service Layout").
TEST(BleAdvertisementTest, GattUuidsMatchContract) {
    EXPECT_EQ(sst::control::BootstrapDefaults::kGattServiceUuid,
              "a1b2c3d4-0001-0000-8000-00805f9b34fb");
    EXPECT_EQ(sst::control::BootstrapDefaults::kGattCommandCharUuid,
              "a1b2c3d4-0011-0000-8000-00805f9b34fb");
    EXPECT_EQ(sst::control::BootstrapDefaults::kGattResponseCharUuid,
              "a1b2c3d4-0012-0000-8000-00805f9b34fb");
    EXPECT_EQ(sst::control::BootstrapDefaults::kBleNamePrefix, "sst-cam-");
}

// INV6 / AE7 (in-container, no bus): the transport is the ILinkSecurity source
// for the credential gate. With no central connected it must report the
// credential link as INSECURE (fail-closed) — reading the guarded default state
// needs no BlueZ, so this runs in the cross-compile container. The gate is wired
// in production (main.cpp): encryption comes from link-level bonding (Just Works
// agent + Pairable adapter + app createBond), and this predicate reads the
// resulting org.bluez Device1 Paired/Bonded state. The real pairing/encryption is
// the on-metal half of AE7 (sniff shows no cleartext passphrase after bonding).
TEST(BleAdvertisementTest, CredentialLinkInsecureWithNoCentral) {
    sst::adapters::control::BluezBleTransport transport(sst::control::MakeAdvertisedName(1));
    const sst::control::ILinkSecurity& link_security = transport;
    EXPECT_FALSE(link_security.IsCredentialLinkSecure());
    EXPECT_FALSE(transport.IsLinkEncrypted());
}

TEST(BleAdvertisementTest, StartsOnDiscoveredAdapterAndPowersIt) {
    FILE* daemon = ::popen(
        "/usr/bin/qemu-aarch64-static -L /l4t/targetfs "
        "/l4t/targetfs/usr/bin/dbus-daemon --session --fork "
        "--print-address=1 --print-pid=1",
        "r");
    ASSERT_NE(daemon, nullptr);
    constexpr std::size_t kAddressBufferSize = 256;
    constexpr std::size_t kPidBufferSize = 64;
    std::array<char, kAddressBufferSize> address{};
    std::array<char, kPidBufferSize> pid{};
    ASSERT_NE(::fgets(address.data(), static_cast<int>(address.size()), daemon), nullptr);
    ASSERT_NE(::fgets(pid.data(), static_cast<int>(pid.size()), daemon), nullptr);
    ::pclose(daemon);
    int daemon_pid = std::atoi(pid.data());
    auto daemon_guard = std::unique_ptr<const int, std::function<void(const int*)>>(
        &daemon_pid, [](const int* process) { ::kill(*process, SIGTERM); });
    const std::string bus_address =
        std::string{address.data()}.substr(0, std::string{address.data()}.find('\n'));
    auto bluez = sdbus::createSessionBusConnectionWithAddress(bus_address);
    bluez->requestName("org.bluez");
    auto root = sdbus::createObject(*bluez, "/");
    root->addObjectManager();
    root->finishRegistration();
    bluez->enterEventLoopAsync();
    std::string transport_name;
    sst::adapters::control::BluezBleTransport transport("sst-cam-0001", "/org/bluez/hci0", [&] {
        auto connection = sdbus::createSessionBusConnectionWithAddress(bus_address);
        transport_name = connection->getUniqueName();
        return connection;
    });
    transport.Start();
    EXPECT_FALSE(transport.IsRunning());
    auto adapter = sdbus::createObject(*bluez, "/org/bluez/hci2");
    bool powered = false;
    adapter->registerProperty("Powered")
        .onInterface("org.bluez.Adapter1")
        .withGetter([&] { return powered; })
        .withSetter([&](bool value) { powered = value; });
    adapter->registerProperty("Alias")
        .onInterface("org.bluez.Adapter1")
        .withGetter([] { return std::string{""}; })
        .withSetter([](const std::string&) {});
    adapter->registerProperty("Pairable")
        .onInterface("org.bluez.Adapter1")
        .withGetter([] { return false; })
        .withSetter([](bool) {});
    std::atomic<int> advertisements{0};
    std::atomic<int> applications{0};
    std::atomic<bool> reject_advertisement{false};
    adapter->registerMethod("RegisterAdvertisement")
        .onInterface("org.bluez.LEAdvertisingManager1")
        .implementedAs(
            [&](const sdbus::ObjectPath& path, const std::map<std::string, sdbus::Variant>&) {
                auto query = sdbus::createSessionBusConnectionWithAddress(bus_address);
                auto proxy = sdbus::createProxy(*query, transport_name, path);
                std::map<std::string, sdbus::Variant> props;
                proxy->callMethod("GetAll")
                    .onInterface("org.freedesktop.DBus.Properties")
                    .withArguments(std::string{"org.bluez.LEAdvertisement1"})
                    .storeResultsTo(props);
                if (props.at("LocalName").get<std::string>() != "sst-cam-0001") {
                    throw sdbus::Error("org.bluez.Error.Failed", "invalid advertisement");
                }
                if (reject_advertisement) {
                    throw sdbus::Error("org.bluez.Error.Failed", "advertisement rejected");
                }
                ++advertisements;
            });
    adapter->registerMethod("RegisterApplication")
        .onInterface("org.bluez.GattManager1")
        .implementedAs(
            [&](const sdbus::ObjectPath& path, const std::map<std::string, sdbus::Variant>&) {
                auto query = sdbus::createSessionBusConnectionWithAddress(bus_address);
                auto proxy = sdbus::createProxy(*query, transport_name, path);
                std::map<sdbus::ObjectPath,
                         std::map<std::string, std::map<std::string, sdbus::Variant>>>
                    objects;
                proxy->callMethod("GetManagedObjects")
                    .onInterface("org.freedesktop.DBus.ObjectManager")
                    .storeResultsTo(objects);
                if (!objects.contains("/com/sst/cam/gatt/service0/char0") ||
                    !objects.contains("/com/sst/cam/gatt/service0/char1")) {
                    throw sdbus::Error("org.bluez.Error.Failed", "invalid GATT application");
                }
                ++applications;
            });
    adapter->registerMethod("UnregisterAdvertisement")
        .onInterface("org.bluez.LEAdvertisingManager1")
        .implementedAs([](const sdbus::ObjectPath&) {});
    adapter->registerMethod("UnregisterApplication")
        .onInterface("org.bluez.GattManager1")
        .implementedAs([](const sdbus::ObjectPath&) {});
    adapter->finishRegistration();
    transport.Start();
    EXPECT_TRUE(transport.IsRunning());
    EXPECT_TRUE(powered);
    EXPECT_GE(advertisements, 1);
    EXPECT_GE(applications, 1);
    auto advertisement = sdbus::createProxy(*bluez, transport_name, "/com/sst/cam/adv");
    advertisement->callMethod("Release").onInterface("org.bluez.LEAdvertisement1");
    EXPECT_FALSE(transport.IsRunning());
    transport.Start();
    EXPECT_TRUE(transport.IsRunning());
    adapter.reset();
    EXPECT_FALSE(transport.IsRunning());
    adapter = sdbus::createObject(*bluez, "/org/bluez/hci2");
    adapter->registerProperty("Powered")
        .onInterface("org.bluez.Adapter1")
        .withGetter([&] { return powered; })
        .withSetter([&](bool value) { powered = value; });
    adapter->registerProperty("Alias")
        .onInterface("org.bluez.Adapter1")
        .withGetter([] { return std::string{""}; })
        .withSetter([](const std::string&) {});
    adapter->registerProperty("Pairable")
        .onInterface("org.bluez.Adapter1")
        .withGetter([] { return false; })
        .withSetter([](bool) {});
    adapter->registerMethod("RegisterAdvertisement")
        .onInterface("org.bluez.LEAdvertisingManager1")
        .implementedAs([&](const sdbus::ObjectPath&, const std::map<std::string, sdbus::Variant>&) {
            if (reject_advertisement) {
                throw sdbus::Error("org.bluez.Error.Failed", "advertisement rejected");
            }
            ++advertisements;
        });
    adapter->registerMethod("RegisterApplication")
        .onInterface("org.bluez.GattManager1")
        .implementedAs([&](const sdbus::ObjectPath&, const std::map<std::string, sdbus::Variant>&) {
            ++applications;
        });
    adapter->registerMethod("UnregisterAdvertisement")
        .onInterface("org.bluez.LEAdvertisingManager1")
        .implementedAs([](const sdbus::ObjectPath&) {});
    adapter->registerMethod("UnregisterApplication")
        .onInterface("org.bluez.GattManager1")
        .implementedAs([](const sdbus::ObjectPath&) {});
    adapter->finishRegistration();
    transport.Start();
    EXPECT_TRUE(transport.IsRunning());
    bluez->releaseName("org.bluez");
    constexpr int kPollAttempts = 100;
    constexpr auto kPollInterval = std::chrono::milliseconds{10};
    for (int attempt = 0; attempt < kPollAttempts && transport.IsRunning(); ++attempt) {
        std::this_thread::sleep_for(kPollInterval);
    }
    EXPECT_FALSE(transport.IsRunning());
    bluez->requestName("org.bluez");
    transport.Start();
    EXPECT_TRUE(transport.IsRunning());
    reject_advertisement = true;
    std::promise<void> disconnect_started;
    std::promise<void> disconnect_allowed;
    auto allow_disconnect = disconnect_allowed.get_future().share();
    transport.SetOnDisconnect([&] {
        disconnect_started.set_value();
        allow_disconnect.wait();
    });
    auto command_char =
        sdbus::createProxy(*bluez, transport_name, "/com/sst/cam/gatt/service0/char0");
    sst_cam::ChunkedPayload ack;
    ack.set_correlation_id("disconnect");
    const auto wire = ack.SerializeAsString();
    command_char->callMethod("WriteValue")
        .onInterface("org.bluez.GattCharacteristic1")
        .withArguments(std::vector<std::uint8_t>{wire.begin(), wire.end()},
                       std::map<std::string, sdbus::Variant>{});
    auto response = sdbus::createProxy(*bluez, transport_name, "/com/sst/cam/gatt/service0/char1");
    response->callMethod("StopNotify").onInterface("org.bluez.GattCharacteristic1");
    EXPECT_EQ(disconnect_started.get_future().wait_for(std::chrono::seconds{3}),
              std::future_status::ready);
    auto stopped = std::async(std::launch::async, [&] { transport.Stop(); });
    const auto stop_status = stopped.wait_for(std::chrono::milliseconds{100});
    disconnect_allowed.set_value();
    EXPECT_EQ(stop_status, std::future_status::timeout);
    EXPECT_EQ(stopped.wait_for(std::chrono::seconds{3}), std::future_status::ready);
    stopped.get();
    EXPECT_FALSE(transport.IsRunning());
    reject_advertisement = false;
    transport.Start();
    EXPECT_TRUE(transport.IsRunning());
    transport.SetOnDisconnect({});
    command_char = sdbus::createProxy(*bluez, transport_name, "/com/sst/cam/gatt/service0/char0");
    command_char->callMethod("WriteValue")
        .onInterface("org.bluez.GattCharacteristic1")
        .withArguments(std::vector<std::uint8_t>{wire.begin(), wire.end()},
                       std::map<std::string, sdbus::Variant>{});
    const int advertisements_before = advertisements;
    reject_advertisement = true;
    response = sdbus::createProxy(*bluez, transport_name, "/com/sst/cam/gatt/service0/char1");
    response->callMethod("StopNotify").onInterface("org.bluez.GattCharacteristic1");
    for (int attempt = 0; attempt < kPollAttempts && transport.IsRunning(); ++attempt) {
        std::this_thread::sleep_for(kPollInterval);
    }
    EXPECT_FALSE(transport.IsRunning());
    EXPECT_EQ(advertisements, advertisements_before);
    reject_advertisement = false;
    transport.Start();
    EXPECT_TRUE(transport.IsRunning());
    transport.Stop();
    bluez->leaveEventLoop();
    adapter.reset();
    root.reset();
    bluez.reset();
}

// Hardware-bound (fails in-container, passes on-device with real BlueZ):
// the transport brings up advertising + the GATT app on the system bus.
TEST(BleAdvertisementTest, AdvertisesOnRealBluez) {
    sst::adapters::control::BluezBleTransport transport(sst::control::MakeAdvertisedName(1));
    transport.Start();
    EXPECT_TRUE(transport.IsRunning())
        << "BlueZ advertising did not come up (expected to fail off-device)";
    transport.Stop();
    EXPECT_FALSE(transport.IsRunning());
}

}  // namespace
