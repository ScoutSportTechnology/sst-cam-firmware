#include "domain/pipeline/services/frame-pair-ring.hpp"

#include <stdexcept>
#include <utility>

namespace sst::pipeline {
FramePairRing::FramePairRing(std::size_t max_pairs, std::size_t max_bytes)
    : max_pairs_(max_pairs), max_bytes_(max_bytes) {
    if (max_pairs == 0 || max_bytes == 0) {
        throw std::invalid_argument("pair ring limits must be positive");
    }
}
auto FramePairRing::Push(FramePair pair) -> bool {
    if (pair.cam0.bytes > max_bytes_ || pair.cam1.bytes > max_bytes_ - pair.cam0.bytes ||
        pair.pts.count() < 0 || (!pairs_.empty() && pair.pts <= pairs_.back().pts)) {
        ++stats_.dropped;
        return false;
    }
    const auto bytes = pair.cam0.bytes + pair.cam1.bytes;
    while (!pairs_.empty() && (pairs_.size() >= max_pairs_ || stats_.bytes > max_bytes_ - bytes)) {
        stats_.bytes -= pairs_.front().cam0.bytes + pairs_.front().cam1.bytes;
        pairs_.pop_front();
        ++stats_.dropped;
        pair.discontinuity = true;
    }
    stats_.bytes += bytes;
    pairs_.push_back(std::move(pair));
    stats_.depth = pairs_.size();
    return true;
}
auto FramePairRing::PopDue(std::chrono::nanoseconds now,
                           std::chrono::nanoseconds delay) -> std::optional<FramePair> {
    if (delay.count() < 0 || pairs_.empty() || now < delay || pairs_.front().pts > now - delay) {
        return std::nullopt;
    }
    auto pair = std::move(pairs_.front());
    pairs_.pop_front();
    stats_.bytes -= pair.cam0.bytes + pair.cam1.bytes;
    stats_.depth = pairs_.size();
    return pair;
}
auto FramePairRing::Stats() const -> FramePairRingStats { return stats_; }
auto FramePairRing::Clear() -> void {
    pairs_.clear();
    stats_.depth = 0;
    stats_.bytes = 0;
}
}
