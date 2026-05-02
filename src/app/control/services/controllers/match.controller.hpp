#pragma once

#include <cstdint>
#include <string_view>

#include "app/control/ports/controller.hpp"
#include "app/db/ports/match-repository.hpp"
#include "app/db/ports/recording-repository.hpp"
#include "app/match/ports/match-service.hpp"

namespace sst::control {

// Routes "match.*" BLE commands. Payloads are JSON (parsed with nlohmann_json).
// Write verbs go through IMatchService (state machine + recording delegation).
// Read verbs hit the repos directly — they have no orchestration logic, only
// shape conversion to JSON.
//
// Write verbs:
//
//   start          {"sport_code":"soccer","team_a_id":1,"team_b_id":2,"name":"Final"?}
//                  → response payload = "<match_id>\n<full_game_recording_id>"
//   set_period     <integer>
//   set_score      {"team":"a"|"b","score":<int>}
//   record_event   {"event_code":"goal","metadata":{<sport-specific>}?}
//                  → response payload = "<match_event_id>\n<event_clip_id>\n<file_path>"
//   end            (no payload) → response payload = "<merged_full_game_path>"
//
// Read verbs:
//
//   list                       → JSON array of match summaries for the user,
//                                most recent started_at first
//   get <match_id>             → JSON {match, events} — full row + ordered
//                                event timeline
//   list_clips <match_id>      → JSON array of event clips for the match
//                                (joins event_clip → match_event)
//   get_clip <event_clip_id>   → JSON of one event_clip row plus its parent
//                                recording (file_path is the mp4 on disk)
class MatchController final : public IController {
   public:
    MatchController(sst::match::IMatchService& service, sst::db::IMatchRepository& match_repo,
                    sst::db::IRecordingRepository& recording_repo, std::int64_t user_id);

    auto Id() const -> std::string_view override { return "match"; }
    auto Handle(const Command& cmd) -> CommandResult override;

   private:
    sst::match::IMatchService& service_;
    sst::db::IMatchRepository& match_repo_;
    sst::db::IRecordingRepository& recording_repo_;
    std::int64_t user_id_;
};

}  // namespace sst::control
