#pragma once

#include <string_view>

namespace sst::control {

// Bootstrap constants for the BLE control plane. The phone uses these to find
// the device and look up the GATT characteristics it talks to.
//
// WiFi-Direct credentials live in DB (table config_wifi_direct, seeded from
// config/wifi-direct.json on first boot) — the phone fetches them by sending
// a "network.bootstrap_p2p" BLE command and reading them back from the
// response payload.
struct BootstrapDefaults {
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
