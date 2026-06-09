#pragma once

#include <cstdint>

namespace sst::session {

// Ordered session lifecycle phase (F1–F3). Transitions advance only in order:
// Idle → Connected → WifiReady → Configured → Ready → Recording. A BLE
// disconnect drops straight back to Idle from any phase.
enum class SessionPhase : std::uint8_t {
    kIdle = 0,        // no central connected
    kConnected = 1,   // BLE connected, no WiFi group yet
    kWifiReady = 2,   // autonomous WiFi Direct group up
    kConfigured = 3,  // session config received + output dirs prepared
    kReady = 4,       // overlay layout applied — ready to record/stream
    kRecording = 5,   // recording in progress
};

}  // namespace sst::session
