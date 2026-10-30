#pragma once

#include <chrono>
#include <functional>

namespace sst::network {

// A one-shot cancellable timer the connectivity arbiter arms on entering
// `kMigrating` (KTD5). It is the backstop for a crashed app that never confirms
// reachability — NOT the primary correctness mechanism (the migration-id epoch
// guard is). The real adapter fires the callback from its own thread; the
// arbiter's epoch check makes a late fire after a confirm a safe no-op.
//
// Contract:
//   - Arm replaces any pending timer (re-arming is safe).
//   - Disarm cancels a pending fire and MUST NOT block waiting for an
//     in-flight callback to finish — the arbiter calls Disarm while holding its
//     own mutex, which the callback also takes, so a blocking Disarm would
//     deadlock. A callback already past the point of no return is tolerated
//     because the arbiter's epoch guard neutralises it.
class ISelfRevertTimer {
   public:
    ISelfRevertTimer() = default;
    virtual ~ISelfRevertTimer() = default;

    ISelfRevertTimer(const ISelfRevertTimer&) = delete;
    auto operator=(const ISelfRevertTimer&) -> ISelfRevertTimer& = delete;
    ISelfRevertTimer(ISelfRevertTimer&&) = delete;
    auto operator=(ISelfRevertTimer&&) -> ISelfRevertTimer& = delete;

    virtual auto Arm(std::chrono::milliseconds after, std::function<void()> on_expire) -> void = 0;
    virtual auto Disarm() -> void = 0;
};

}  // namespace sst::network
