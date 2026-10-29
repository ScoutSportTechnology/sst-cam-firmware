#pragma once

#include <cstdint>

namespace sst::session {

enum class SessionPhase : std::uint8_t {
    kIdle = 0,        // no central connected
    kConnected = 1,   // BLE connected, no WiFi group yet
    kWifiReady = 2,   // autonomous WiFi Direct group up
    kConfigured = 3,  // session config received + output dirs prepared
    kReady = 4,       // overlay layout applied — ready to record/stream
    kRecording = 5,   // recording in progress
    kFinalizing = 6,
};

}  // namespace sst::session
