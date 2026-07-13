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
using MigrationId = std::uint32_t;

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
