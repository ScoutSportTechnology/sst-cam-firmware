#include "adapters/network/timer/thread-self-revert-timer.hpp"

#include <mutex>
#include <utility>

namespace sst::adapters::network {

ThreadSelfRevertTimer::ThreadSelfRevertTimer() : worker_([this] { Run(); }) {}

ThreadSelfRevertTimer::~ThreadSelfRevertTimer() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stop_ = true;
        armed_ = false;
        ++generation_;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

auto ThreadSelfRevertTimer::Arm(std::chrono::milliseconds after,
                                std::function<void()> on_expire) -> void {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        ++generation_;  // invalidate any pending fire
        armed_ = true;
        deadline_ = std::chrono::steady_clock::now() + after;
        on_expire_ = std::move(on_expire);
    }
    cv_.notify_all();
}

auto ThreadSelfRevertTimer::Disarm() -> void {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        ++generation_;  // a pending fire on the old generation becomes a no-op
        armed_ = false;
        on_expire_ = nullptr;
    }
    cv_.notify_all();
    // Deliberately does NOT join/wait on an in-flight callback (see the port
    // contract): the arbiter holds a lock the callback needs.
}

auto ThreadSelfRevertTimer::Run() -> void {
    std::unique_lock<std::mutex> lock(mtx_);
    while (true) {
        cv_.wait(lock, [this] { return stop_ || armed_; });
        if (stop_) {
            return;
        }

        const std::uint64_t gen = generation_;
        const auto deadline = deadline_;
        // Wait until the deadline, or until the timer is re-armed/disarmed/stopped
        // (generation moves). A spurious/early wake re-loops.
        cv_.wait_until(lock, deadline, [this, gen] { return stop_ || generation_ != gen; });

        if (stop_) {
            return;
        }
        if (generation_ != gen) {
            continue;  // superseded by Arm/Disarm — re-evaluate
        }
        if (!armed_ || std::chrono::steady_clock::now() < deadline) {
            continue;
        }

        // Genuine expiry on the current generation. Capture the callback, mark
        // disarmed, and invoke it WITHOUT the lock held.
        auto callback = on_expire_;
        armed_ = false;
        on_expire_ = nullptr;
        ++generation_;
        lock.unlock();
        if (callback) {
            callback();
        }
        lock.lock();
    }
}

}  // namespace sst::adapters::network
