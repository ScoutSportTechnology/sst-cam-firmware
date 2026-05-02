#pragma once

#include <string_view>

#include "app/control/ports/controller.hpp"
#include "app/db/ports/team-repository.hpp"

namespace sst::control {

// Routes "team.*" BLE commands. Teams are scoped to a sport (composite FK
// from match→team enforces the team plays the right sport); players are
// scoped to a team.
//
// Verbs:
//
//   create               JSON {"sport_id":1,"name":"Alpha","short_name":"A"}
//                         → response payload = "<team_id>"
//   list <sport_id>      → response payload = "<id> <name> <short_name>\n..."
//   delete <id>          → kOk on success, kNotFound if no row matched
//
//   add_player           JSON {"team_id":1,"name":"Striker","jersey_number":9?}
//                         → response payload = "<player_id>"
//   list_players <team_id>
//                         → response payload = "<id> <name> <jersey_number_or_->\n..."
//   remove_player <id>   → kOk on success, kNotFound if no row matched
class TeamController final : public IController {
   public:
    explicit TeamController(sst::db::ITeamRepository& repo);

    auto Id() const -> std::string_view override { return "team"; }
    auto Handle(const Command& cmd) -> CommandResult override;

   private:
    sst::db::ITeamRepository& repo_;
};

}  // namespace sst::control
