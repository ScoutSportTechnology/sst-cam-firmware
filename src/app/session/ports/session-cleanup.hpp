#pragma once

namespace sst::session {

// Disconnect/teardown fan-out targets (R15, AE2). On session end the
// SessionManager invokes all of these — order-independent; each must be
// idempotent and a no-op when the corresponding subsystem is inactive. The
// concrete implementation (U14) wires these to the recording service (finalize
// the continuous MP4), the streaming service (close RTMP), and the WiFi Direct
// controller (tear down the group).
class ISessionCleanup {
   public:
    virtual ~ISessionCleanup() = default;

    virtual auto FinalizeRecording() -> void = 0;
    virtual auto StopStreaming() -> void = 0;
    virtual auto TeardownWifiDirect() -> void = 0;
};

}  // namespace sst::session
