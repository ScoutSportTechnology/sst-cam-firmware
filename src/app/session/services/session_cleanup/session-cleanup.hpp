#pragma once

#include "app/control/ports/dhcp-server.hpp"
#include "app/control/ports/wifi-manager.hpp"
#include "app/session/ports/session-cleanup.hpp"
#include "app/storage/ports/recording-service.hpp"
#include "app/streaming/ports/streaming-service.hpp"

namespace sst::session {

// Concrete disconnect/teardown fan-out (U14 wiring). Each action is idempotent
// and a no-op when its subsystem is inactive, so the SessionManager can invoke
// them unconditionally and order-independently (R15).
class SessionCleanup final : public ISessionCleanup {
   public:
    SessionCleanup(sst::storage::IRecordingService& recording,
                   sst::streaming::IStreamingService& streaming, sst::control::IWifiManager& wifi,
                   sst::control::IDhcpServer& dhcp);

    auto FinalizeRecording() -> void override;
    auto StopStreaming() -> void override;
    auto TeardownWifiDirect() -> void override;

   private:
    sst::storage::IRecordingService& recording_;
    sst::streaming::IStreamingService& streaming_;
    sst::control::IWifiManager& wifi_;
    sst::control::IDhcpServer& dhcp_;
};

}  // namespace sst::session
