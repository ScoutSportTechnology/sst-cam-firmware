#pragma once

namespace sst::control {

// Port: is the BLE link that currently carries control traffic encrypted and
// bonded? INV6 / AE7 require that WiFi credentials never traverse a cleartext
// link. Because the proto contract mandates a single Command-Write
// characteristic (there is no separate credential characteristic), the
// credential-bearing SetNetworkConfigCommand rides the same characteristic as
// every other command. The transport-layer GATT flag
// (`encrypt-authenticated-write`) is the primary enforcement; this port is the
// app-layer defense-in-depth double-check, so NetworkHandler can reject a
// SetNetworkConfig that somehow arrived on an unencrypted/unbonded link.
//
// Implemented by the BlueZ transport adapter, which tracks the org.bluez
// Device1 Connected / Paired / Bonded state of the connected central.
class ILinkSecurity {
   public:
    ILinkSecurity() = default;
    virtual ~ILinkSecurity() = default;

    ILinkSecurity(const ILinkSecurity&) = delete;
    auto operator=(const ILinkSecurity&) -> ILinkSecurity& = delete;
    ILinkSecurity(ILinkSecurity&&) = delete;
    auto operator=(ILinkSecurity&&) -> ILinkSecurity& = delete;

    // True when a central is connected over a paired/bonded (hence encrypted)
    // link. False when no central is connected, or the link is unpaired/cleartext.
    [[nodiscard]] virtual auto IsCredentialLinkSecure() const -> bool = 0;
};

}  // namespace sst::control
