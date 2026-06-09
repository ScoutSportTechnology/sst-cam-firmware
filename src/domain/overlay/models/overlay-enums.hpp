#pragma once

#include <cstdint>

namespace sst::overlay {

// Shape of an overlay element (mirrors proto OverlayShape, KTD2 — proto types
// never cross into domain).
enum class OverlayShape : std::uint8_t { kUnknown = 0, kRect = 1, kText = 2, kCircle = 3 };

// Live data source for a TEXT element (mirrors proto OverlayBinding).
enum class OverlayBinding : std::uint8_t {
    kStatic = 0,
    kScoreA = 1,
    kScoreB = 2,
    kScoreVs = 3,
    kTeamAName = 4,
    kTeamBName = 5,
    kMatchClock = 6,
    kPeriodLabel = 7,
};

enum class TextAlign : std::uint8_t { kLeft = 0, kCenter = 1, kRight = 2 };

enum class FontWeight : std::uint8_t { kNormal = 0, kBold = 1 };

// Period-label state for BINDING_PERIOD_LABEL: in-play renders "P{n}",
// half-time "HT", full-time "FT".
enum class PeriodLabelState : std::uint8_t { kInPlay = 0, kHalfTime = 1, kFullTime = 2 };

}  // namespace sst::overlay
