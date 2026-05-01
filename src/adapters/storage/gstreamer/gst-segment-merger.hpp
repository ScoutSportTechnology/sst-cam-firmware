#pragma once

#include <filesystem>
#include <vector>

namespace sst::adapters::storage {

// One-shot pipeline that concatenates a list of mp4 segments produced by
// splitmuxsink into a single mp4, copy-only — no re-encode. Used by the
// segment recorder when Stop() is called: concat all `seg_*.mp4` files into
// `full_game.mp4`.
class GstSegmentMerger {
   public:
    GstSegmentMerger();
    ~GstSegmentMerger() = default;

    GstSegmentMerger(const GstSegmentMerger&) = delete;
    auto operator=(const GstSegmentMerger&) -> GstSegmentMerger& = delete;
    GstSegmentMerger(GstSegmentMerger&&) = delete;
    auto operator=(GstSegmentMerger&&) -> GstSegmentMerger& = delete;

    // Returns true on success. `output` is overwritten if it exists.
    auto Merge(const std::vector<std::filesystem::path>& segments,
               const std::filesystem::path& output) -> bool;
};

}  // namespace sst::adapters::storage
