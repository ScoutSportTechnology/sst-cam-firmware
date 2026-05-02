#pragma once

#include <string_view>

#include "app/control/ports/controller.hpp"
#include "app/db/ports/sport-repository.hpp"

namespace sst::control {

// Routes "sport.*" BLE commands. Sports + their event kinds are the catalog
// the MatchController validates against — this controller is how the
// companion app populates that catalog at runtime.
//
// Verbs (verb is everything up to the first space; the rest is the arg):
//
//   create               JSON {"code":"soccer","name":"Soccer","periods":2}
//                         → response payload = "<sport_id>"
//   list                 → response payload = "<id> <code> <name> <periods>\n..."
//   delete <id>          → kOk on success, kNotFound if no row matched
//
//   add_event_kind       JSON {"sport_id":1,"code":"goal","name":"Goal"}
//                         → kOk
//   list_event_kinds <sport_id>
//                         → response payload = "<code> <name>\n..."
//   remove_event_kind <sport_id> <code>
//                         → kOk on success, kNotFound if no row matched
class SportController final : public IController {
   public:
    explicit SportController(sst::db::ISportRepository& repo);

    auto Id() const -> std::string_view override { return "sport"; }
    auto Handle(const Command& cmd) -> CommandResult override;

   private:
    sst::db::ISportRepository& repo_;
};

}  // namespace sst::control
