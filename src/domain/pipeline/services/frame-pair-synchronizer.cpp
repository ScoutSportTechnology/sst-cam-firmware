#include "domain/pipeline/services/frame-pair-synchronizer.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace sst::pipeline {
FramePairSynchronizer::FramePairSynchronizer(int fps, std::chrono::nanoseconds extra_skew) {
    if (fps <= 0 || extra_skew.count() < 0) {
        throw std::invalid_argument("invalid pairing timing");
    }
    interval_ = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)) / fps;
    max_skew_ = interval_ / 2 + extra_skew;
}

auto FramePairSynchronizer::Push(std::uint16_t camera_index,
                                 sst::capture::VideoSurface surface) -> std::optional<FramePair> {
    if (camera_index > 1 || surface.camera_index != camera_index || !surface.owner ||
        surface.clock_epoch == 0 || surface.capture_pts.count() <= 0 || surface.bytes == 0) {
        synchronized_ = false;
        return std::nullopt;
    }
    if (surface.discontinuity || (epochs_[camera_index] != std::optional<std::uint64_t>{} &&
                                  epochs_[camera_index] != surface.clock_epoch)) {
        Restart(camera_index);
    }
    auto& last = last_pts_[camera_index];
    if (last.has_value() && surface.capture_pts <= last.value()) {
        ++stats_.dropped[camera_index];
        return std::nullopt;
    }
    if (epochs_[1 - camera_index] != std::optional<std::uint64_t>{} &&
        epochs_[1 - camera_index] != surface.clock_epoch) {
        pending_[0].clear();
        pending_[1].clear();
        synchronized_ = false;
        discontinuity_ = true;
        ++stats_.dropped[camera_index];
        epochs_[camera_index] = surface.clock_epoch;
        last = surface.capture_pts;
        return std::nullopt;
    }
    epochs_[camera_index] = surface.clock_epoch;
    last = surface.capture_pts;
    auto& own = pending_[camera_index];
    auto& other = pending_[1 - camera_index];
    own.push_back(std::move(surface));
    while (own.size() > kMaxPendingPerCamera) {
        own.pop_front();
        ++stats_.dropped[camera_index];
        discontinuity_ = true;
    }
    while (!own.empty() && !other.empty()) {
        auto& current = own.front();
        auto best =
            std::min_element(other.begin(), other.end(), [&](const auto& left, const auto& right) {
                return std::chrono::abs(left.capture_pts - current.capture_pts) <
                       std::chrono::abs(right.capture_pts - current.capture_pts);
            });
        auto skew = std::chrono::abs(best->capture_pts - current.capture_pts);
        if (skew <= max_skew_) {
            const auto skipped = static_cast<std::uint64_t>(std::distance(other.begin(), best));
            auto matched = std::move(*best);
            other.erase(other.begin(), std::next(best));
            stats_.dropped[1 - camera_index] += skipped;
            discontinuity_ = discontinuity_ || skipped != 0;
            auto first = std::move(current);
            own.pop_front();
            FramePair pair;
            if (camera_index == 0) {
                pair.cam0 = std::move(first);
                pair.cam1 = std::move(matched);
            } else {
                pair.cam0 = std::move(matched);
                pair.cam1 = std::move(first);
            }
            pair.pts = pair.cam0.capture_pts;
            pair.skew = skew;
            pair.discontinuity = std::exchange(discontinuity_, false);
            ++stats_.paired;
            synchronized_ = true;
            spdlog::info("pair pts={} raw={},{} skew_ns={} discontinuity={}", pair.pts.count(),
                         pair.cam0.raw_pts.count(), pair.cam1.raw_pts.count(), skew.count(),
                         pair.discontinuity);
            return pair;
        }
        const auto older =
            current.capture_pts < other.front().capture_pts ? camera_index : 1 - camera_index;
        pending_[older].pop_front();
        ++stats_.dropped[older];
        discontinuity_ = true;
    }
    return std::nullopt;
}

auto FramePairSynchronizer::Restart(std::uint16_t camera_index) -> void {
    if (camera_index > 1) {
        return;
    }
    pending_[0].clear();
    pending_[1].clear();
    last_pts_[camera_index].reset();
    epochs_[camera_index].reset();
    synchronized_ = false;
    discontinuity_ = true;
}
auto FramePairSynchronizer::Synchronized() const -> bool { return synchronized_; }
auto FramePairSynchronizer::Stats() const -> PairStats {
    auto result = stats_;
    result.pending = {pending_[0].size(), pending_[1].size()};
    return result;
}
}  // namespace sst::pipeline
