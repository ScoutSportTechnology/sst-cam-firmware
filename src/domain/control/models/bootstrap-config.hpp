#pragma once

#include <string_view>

namespace sst::control {

// Bootstrap constants for the BLE control plane. The companion app uses the
// service UUID (advertised) plus the `sst-cam-` name prefix as a two-layer scan
// filter; once connected it looks up the two characteristics by UUID.
//
// These UUIDs come straight from the sst-cam-proto contract (proto/README.md,
// "GATT Service Layout"). They are marked placeholders there — replace with
// officially registered UUIDs before production, in lockstep with the contract.
//
// The advertised device name is NOT a constant: it is computed per unit as
// `sst-cam-NNNN` from the device identity in config (see
// domain/control/utils/advertised-name.hpp).
struct BootstrapDefaults {
    static constexpr std::string_view kGattServiceUuid =
        "a1b2c3d4-0001-0000-8000-00805f9b34fb";
    static constexpr std::string_view kGattCommandCharUuid =
        "a1b2c3d4-0011-0000-8000-00805f9b34fb";
    static constexpr std::string_view kGattResponseCharUuid =
        "a1b2c3d4-0012-0000-8000-00805f9b34fb";

    // Device-name prefix for the app's secondary scan filter: `sst-cam-NNNN`.
    static constexpr std::string_view kBleNamePrefix = "sst-cam-";
};

}  // namespace sst::control
