#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

#include "app/network/ports/self-revert-timer.hpp"

namespace sst::adapters::network {

// A single-worker-thread ISelfRevertTimer. Arm/Disarm bump a generation counter
// so a re-arm or cancel invalidates any pending fire; the worker invokes the
// expiry callback WITHOUT holding the timer's mutex, so the arbiter (which calls
// Disarm under its own lock, a lock the callback also takes) can never deadlock.
// A callback that fires a hair before Disarm is neutralised by the arbiter's
// epoch guard.
class ThreadSelfRevertTimer final : public sst::network::ISelfRevertTimer {
   public:
    ThreadSelfRevertTimer();
    ~ThreadSelfRevertTimer() override;

    auto Arm(std::chrono::milliseconds after, std::function<void()> on_expire) -> void override;
    auto Disarm() -> void override;

   private:
    auto Run() -> void;

    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_{false};
    bool armed_{false};
    std::uint64_t generation_{0};
    std::chrono::steady_clock::time_point deadline_;
    std::function<void()> on_expire_;
    std::thread worker_;
};

}  // namespace sst::adapters::network
