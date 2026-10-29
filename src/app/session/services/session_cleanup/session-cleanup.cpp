#include "app/session/services/session_cleanup/session-cleanup.hpp"

namespace sst::session {

SessionCleanup::SessionCleanup(sst::storage::IRecordingService& recording,
                               sst::streaming::IStreamingService& streaming,
                               sst::raw_capture::IRawCaptureSink& proxy,
                               sst::control::IWifiManager& wifi, sst::control::IDhcpServer& dhcp,
                               sst::decision::ManualCameraState& camera_state,
                               sst::streaming::PreviewLayoutState& layout_state,
                               sst::overlay::IOverlayTimelineRecorder& timeline)
    : recording_(recording),
      streaming_(streaming),
      proxy_(proxy),
      timeline_(timeline),

      wifi_(wifi),
      dhcp_(dhcp),
      camera_state_(camera_state),
      layout_state_(layout_state) {}

auto SessionCleanup::FinalizeRecording() -> bool {
    const auto result = recording_.Stop();
    proxy_.Stop();
    timeline_.Stop();
    return result.success;
}

auto SessionCleanup::StopStreaming() -> void {
    streaming_.StopAppStream();
    for (const auto& active : streaming_.ListActivePlatformStreams()) {
        streaming_.StopPlatformStream(active.stream_id);
    }
}

auto SessionCleanup::TeardownWifiDirect() -> void {
    dhcp_.Stop();
    wifi_.Stop();
}

auto SessionCleanup::ResetSelections() -> void {
    camera_state_.Set(0);
    layout_state_.Set(sst::streaming::PreviewLayout::kSingle);
}

}  // namespace sst::session
