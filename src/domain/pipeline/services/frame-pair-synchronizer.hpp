#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

#include "domain/capture/models/video-surface.hpp"

namespace sst::pipeline {

struct FramePair {
    sst::capture::VideoSurface cam0;
    sst::capture::VideoSurface cam1;
    std::chrono::nanoseconds pts{0};
    std::chrono::nanoseconds skew{0};
    bool discontinuity{false};
};

struct PairStats {
    std::array<std::size_t, 2> pending{};
    std::array<std::uint64_t, 2> dropped{};
    std::uint64_t paired{0};
};

class FramePairSynchronizer {
   public:
    static constexpr int kDefaultFps = 30;
    static constexpr std::size_t kMaxPendingPerCamera = 3;
    explicit FramePairSynchronizer(int fps = kDefaultFps);
    auto Push(std::uint16_t camera_index,
              sst::capture::VideoSurface surface) -> std::optional<FramePair>;
    auto Restart(std::uint16_t camera_index) -> void;
    [[nodiscard]] auto Synchronized() const -> bool;
    [[nodiscard]] auto Stats() const -> PairStats;

   private:
    std::chrono::nanoseconds interval_;
    std::chrono::nanoseconds max_skew_;
    std::array<std::deque<sst::capture::VideoSurface>, 2> pending_;
    std::array<std::optional<std::chrono::nanoseconds>, 2> last_pts_;
    std::array<std::optional<std::uint64_t>, 2> epochs_;
    PairStats stats_;
    bool synchronized_{false};
    bool discontinuity_{false};
};

}  // namespace sst::pipeline
