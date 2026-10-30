#pragma once

#include <cstdint>

namespace sst::network {

// The camera's WiFi connectivity mode — the single fact the connectivity arbiter
// owns to enforce INV2 (never STA + P2P concurrently). The radio is in exactly
// one of these at any instant; Ethernet is orthogonal and does not appear here.
//
//   kP2pOnly   — S1: WiFi-Direct Group Owner, unauth preview at the GO IP,
//                streaming disabled. Boot state (R-BOOT) and the failure/revert
//                fallback.
//   kMigrating — transient: GO torn down, joined the phone's network as STA,
//                per-session preview credential minted, awaiting the app's
//                reachability confirm. A bounded self-revert timer is armed.
//   kStaOnly   — S2: joined the phone's network (P2P down), authenticated
//                preview over the STA LAN IP, streaming allowed. Reached only via
//                an app-confirmed migration.
enum class ConnectivityMode : std::uint8_t {
    kP2pOnly = 0,
    kMigrating = 1,
    kStaOnly = 2,
};

// Monotonic epoch id minted per P2P->STA migration attempt (KTD5). The app's
// reachability confirm/deny carries the id it saw; a verdict for a superseded id
// is ignored so the app-confirm and the self-revert timer can never both act on
// the same migration. 0 = no migration in progress.
//
// U4: this SAME counter is the single global epoch reused for BOTH the STA
// migration handshake AND the Ethernet-LAN handshake — bumped on EVERY
// re-arbitration so a stale confirm (for a superseded selection) can never match
// the live id and finalize a wrong pick. There is deliberately NO second,
// per-path confirm id (a parallel counter could re-mint a value the stale confirm
// still matches; see KTD "single-counter epoch").
using MigrationId = std::uint32_t;

// The single active LAN path the camera uses to reach the phone (R3/R5, proto v5
// LanPath). ORTHOGONAL to ConnectivityMode: it names WHICH interface carries the
// LAN, not the radio state. Ethernet-as-LAN never touches the radio, so INV2
// (never STA + P2P concurrently) stays a radio-only invariant. The connectivity
// arbiter selects exactly ONE of these at a time so the camera never dual-homes
// the same subnet (precedence Ethernet-on-phone-subnet -> WiFi-STA -> P2P).
//
//   kNone     — no LAN path decided yet (boot, before any arbitration).
//   kEthernet — Ethernet is on the phone's subnet and carries the LAN. The radio
//               is untouched (typically the idle GO in kP2pOnly).
//   kWifiSta  — the camera joined the phone's Wi-Fi as STA; STA carries the LAN
//               (radio kStaOnly / kMigrating).
//   kP2p      — no shared LAN; the phone reaches the camera over WiFi-Direct.
enum class LanPath : std::uint8_t {
    kNone = 0,
    kEthernet = 1,
    kWifiSta = 2,
    kP2p = 3,
};

inline auto ToString(LanPath path) -> const char* {
    switch (path) {
        case LanPath::kNone:
            return "None";
        case LanPath::kEthernet:
            return "Ethernet";
        case LanPath::kWifiSta:
            return "WifiSta";
        case LanPath::kP2p:
            return "P2p";
    }
    return "Unknown";
}

inline auto ToString(ConnectivityMode mode) -> const char* {
    switch (mode) {
        case ConnectivityMode::kP2pOnly:
            return "P2pOnly";
        case ConnectivityMode::kMigrating:
            return "Migrating";
        case ConnectivityMode::kStaOnly:
            return "StaOnly";
    }
    return "Unknown";
}

}  // namespace sst::network
