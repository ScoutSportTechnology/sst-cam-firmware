#pragma once

namespace sst::session {

class ISessionCleanup {
   public:
    virtual ~ISessionCleanup() = default;

    virtual auto FinalizeRecording() -> bool = 0;
    virtual auto StopStreaming() -> void = 0;
    virtual auto TeardownWifiDirect() -> void = 0;

    virtual auto ResetSelections() -> void = 0;
};

}  // namespace sst::session
