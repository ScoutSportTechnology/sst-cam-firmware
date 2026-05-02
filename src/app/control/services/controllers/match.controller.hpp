#pragma once

#include <string_view>

#include "app/control/ports/controller.hpp"
#include "app/match/ports/match-service.hpp"

namespace sst::control {

// Routes "match.*" BLE commands. Payloads are JSON (parsed with nlohmann_json).
//
//   start          {"sport_code":"soccer","team_a_id":1,"team_b_id":2,"name":"Final"?}
//                  → response payload = "<match_id>\n<full_game_recording_id>"
//
//   set_period     <integer>
//   set_score      {"team":"a"|"b","score":<int>}
//
//   record_event   {"event_code":"goal","metadata":{<sport-specific>}?}
//                  → response payload = "<match_event_id>\n<event_clip_id>\n<file_path>"
//
//   end            (no payload)
//                  → response payload = "<merged_full_game_path>"
//
class MatchController final : public IController {
   public:
    explicit MatchController(sst::match::IMatchService& service);

    auto Id() const -> std::string_view override { return "match"; }
    auto Handle(const Command& cmd) -> CommandResult override;

   private:
    sst::match::IMatchService& service_;
};

}  // namespace sst::control
