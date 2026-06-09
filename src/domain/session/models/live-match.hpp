#pragma once

#include <cstdint>

namespace sst::session {

// Coarse match segment for the period-label display ("P{n}" / "HT" / "FT").
enum class MatchSegment : std::uint8_t { kInPlay = 0, kHalfTime = 1, kFullTime = 2 };

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
    MatchSegment segment{MatchSegment::kInPlay};
};

}  // namespace sst::session
