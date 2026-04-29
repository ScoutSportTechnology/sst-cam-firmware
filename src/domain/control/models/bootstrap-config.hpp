#pragma once

#include <string_view>

namespace sst::control {

// Defaults used to bootstrap the WiFi-Direct link from a fresh BLE connection.
// The phone learns the actual SSID/passphrase by sending a "network.bootstrap_p2p"
// command over BLE and reading them back from the response payload, so changing
// these values here does not require an app rebuild — but the *fallback* (used
// when neither side has stored creds) MUST match the companion app's hardcoded
// fallback. Keep these in sync with the app repo.
struct BootstrapDefaults {
    static constexpr std::string_view kP2pSsid = "DIRECT-SST-CAM";
    static constexpr std::string_view kP2pPassphrase = "scoutcam1234";

    static constexpr std::string_view kBleAdvertisedName = "SST-CAM";

    // GATT service + characteristics the companion app will look up.
    // Pick any 128-bit UUIDs you like; just keep both repos aligned.
    static constexpr std::string_view kGattServiceUuid =
        "5f9b34fb-0000-4000-8000-00805f9b34fb";
    static constexpr std::string_view kGattCommandCharUuid =
        "5f9b34fb-0001-4000-8000-00805f9b34fb";
    static constexpr std::string_view kGattResponseCharUuid =
        "5f9b34fb-0002-4000-8000-00805f9b34fb";
};

}  // namespace sst::control
