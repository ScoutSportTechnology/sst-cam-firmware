#pragma once

#include <cstdint>

namespace sst::session {

// Display-only live match state held in session memory. The app is the score +
// timing authority (R20); these values reflect only what the app last pushed
// (ScoreUpdate / MatchControl) and are cleared with the session. The clock is
// ticked locally between app events purely for a smooth MM:SS readout.
struct LiveMatch {
    std::uint32_t score_a{0};
    std::uint32_t score_b{0};
    std::uint32_t period{0};
    std::uint32_t clock_seconds{0};
    bool clock_running{false};
};

}  // namespace sst::session
