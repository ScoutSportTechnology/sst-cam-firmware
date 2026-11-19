#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

#include "domain/pipeline/services/frame-pair-synchronizer.hpp"

namespace sst::pipeline {
struct FramePairRingStats {
    std::size_t depth{0};
    std::size_t bytes{0};
    std::uint64_t dropped{0};
};

class FramePairRing {
   public:
    FramePairRing(std::size_t max_pairs, std::size_t max_bytes);
    auto Push(FramePair pair) -> bool;
    auto PopDue(std::chrono::nanoseconds now,
                std::chrono::nanoseconds delay) -> std::optional<FramePair>;
    [[nodiscard]] auto Stats() const -> FramePairRingStats;
    auto Clear() -> void;

   private:
    std::size_t max_pairs_;
    std::size_t max_bytes_;
    std::deque<FramePair> pairs_;
    FramePairRingStats stats_;
};
}
