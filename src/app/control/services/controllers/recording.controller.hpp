#pragma once

#include <string_view>

#include "app/control/ports/controller.hpp"
#include "app/storage/ports/recording-service.hpp"

namespace sst::control {

// Routes "recording.*" BLE commands. Match start / end / event-clip live on
// the match.* route (those depend on match identity); this controller is just
// the in-game pause / resume toggle, which only acts on the segment recorder
// — the underlying encoder + event-clip ring stay live so triggered clips
// still work during a pause.
//
// Verbs (payload is plain text):
//   pause   → IRecordingService::Pause
//   resume  → IRecordingService::Resume
class RecordingController final : public IController {
   public:
    explicit RecordingController(sst::storage::IRecordingService& service);

    auto Id() const -> std::string_view override { return "recording"; }
    auto Handle(const Command& cmd) -> CommandResult override;

   private:
    sst::storage::IRecordingService& service_;
};

}  // namespace sst::control
