#include "domain/pipeline/services/frame-pair-synchronizer.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace sst::pipeline {

FramePairSynchronizer::FramePairSynchronizer(int fps) {
    if (fps <= 0) {
        throw std::invalid_argument("pair fps must be positive");
    }
    interval_ = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)) / fps;
    max_skew_ = interval_ / 2 + std::chrono::milliseconds(2);
}

auto FramePairSynchronizer::Push(std::uint16_t camera_index,
                                 sst::capture::VideoSurface surface) -> std::optional<FramePair> {
    if (camera_index > 1 || surface.camera_index != camera_index || !surface.owner ||
        surface.clock_epoch == 0 || surface.capture_pts.count() < 0 || surface.bytes == 0) {
        synchronized_ = false;
        return std::nullopt;
    }
    if (surface.discontinuity) {
        Restart(camera_index);
    }
    auto& last = last_pts_[camera_index];
    const auto current_epoch = epochs_[camera_index];
    if (current_epoch.has_value() && current_epoch.value() != surface.clock_epoch) {
        Restart(camera_index);
    }
    if (last && surface.capture_pts <= *last) {
        ++stats_.dropped[camera_index];
        return std::nullopt;
    }
    const auto other_epoch = epochs_[1 - camera_index];
    if (other_epoch.has_value() && other_epoch.value() != surface.clock_epoch) {
        synchronized_ = false;
        pending_[0].clear();
        pending_[1].clear();
        epochs_[camera_index] = surface.clock_epoch;
        last = surface.capture_pts;
        ++stats_.dropped[camera_index];
        return std::nullopt;
    }
    epochs_[camera_index] = surface.clock_epoch;
    last = surface.capture_pts;
    auto& own = pending_[camera_index];
    auto& other = pending_[1 - camera_index];
    own.push_back(std::move(surface));
    if (other.empty()) {
        while (own.size() > 1 && own.back().capture_pts - own.front().capture_pts >= interval_) {
            own.pop_front();
            ++stats_.dropped[camera_index];
            discontinuity_ = true;
        }
    }
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
        const auto skew = std::chrono::abs(best->capture_pts - current.capture_pts);
        if (skew <= max_skew_) {
            auto matched = std::move(*best);
            other.erase(other.begin(), std::next(best));
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
            pair.skew = skew;
            pair.discontinuity = std::exchange(discontinuity_, false);
            pair.pts = pair.cam0.capture_pts;
            ++stats_.paired;
            synchronized_ = true;
            spdlog::info("FramePair: pts={} skew_ns={} discontinuity={}", pair.pts.count(),
                         skew.count(), pair.discontinuity);
            return pair;
        }
        const auto older_index =
            current.capture_pts < other.front().capture_pts ? camera_index : 1 - camera_index;
        auto& older = pending_[older_index];
        older.pop_front();
        ++stats_.dropped[older_index];
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
    auto stats = stats_;
    stats.pending = {pending_[0].size(), pending_[1].size()};
    return stats;
}

}  // namespace sst::pipeline
