// BLE GATT contract conformance (U5, R2/R2a/R8).
//
// The name-formatting + UUID-constant checks are pure and run in-container.
// The live-advertising check is hardware-bound (real BlueZ over system D-Bus)
// and is expected to FAIL in the cross-compile container — it passes on-device.

#include <gtest/gtest.h>

#include <string>

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
