#include "app/network/services/connectivity_arbiter/connectivity-arbiter.hpp"

#include <spdlog/spdlog.h>

#include <mutex>
#include <utility>

#include "domain/network/models/formatter/_fmt.hpp"  // IWYU pragma: keep

namespace sst::network {

ConnectivityArbiter::ConnectivityArbiter(sst::control::IWifiManager& wifi,
                                         IUplinkConfigurator& uplink, ISelfRevertTimer& timer,
                                         IConnectivityTransitionHooks& hooks,
                                         std::chrono::milliseconds revert_after)
    : wifi_(wifi), uplink_(uplink), timer_(timer), hooks_(hooks), revert_after_(revert_after) {
    // R-BOOT: the arbiter is born in kP2pOnly and never auto-applies a stored
    // uplink — S2 is only ever reached through an app-confirmed migration.
    spdlog::info("ConnectivityArbiter: boot mode={}", mode_);
}

auto ConnectivityArbiter::RequestSharedWifi(const sst::config::WifiStaUplink& uplink)
    -> std::optional<SharedWifiMigration> {
    std::lock_guard<std::mutex> lock(mtx_);

    // Re-entrancy guard: a migration ALREADY IN FLIGHT is a real transient — never
    // start a second radio transition on top of it. INV2 / AE5. (kStaOnly is NOT
    // rejected here — see the change-networks path below.)
    if (mode_ == ConnectivityMode::kMigrating) {
        spdlog::warn(
            "ConnectivityArbiter: RequestSharedWifi rejected — mode={} (migration in flight)",
            mode_);
        last_error_ = "a network change is already in progress — try again in a moment";
        return std::nullopt;
    }

    // CHANGE-NETWORKS: already in S2 (kStaOnly) and asked to join a network. The
    // handler has already short-circuited a same-network no-op before reaching us,
    // so this is a genuine switch. One radio can't hold two STA sessions, so tear
    // the current STA fully down (release lease, flush address/routes) and restore
    // the GO FIRST — DoRevertLocked does exactly that and leaves us in a clean
    // kP2pOnly to fall through into the normal migrate path with a fresh epoch.
    if (mode_ == ConnectivityMode::kStaOnly) {
        spdlog::info(
            "ConnectivityArbiter: change-networks from kStaOnly (ssid={}) — reverting then "
            "migrating",
            current_ssid_);
        DoRevertLocked();
    }

    // From here mode_ == kP2pOnly. INV2: GO down FIRST, then STA up. The ordering is
    // the whole point — no code path applies STA before the GO teardown returns.
    wifi_.Stop();
    const UplinkResult sta = uplink_.ApplyWifiSta(uplink);
    if (!sta.ok) {
        // STA never fully came up — tear down any partial STA state (lease/address/
        // routes), restore the GO, and stay in a legal S1. detail is the failure
        // reason (surfaced to the app as a friendly message by U7 + error_message).
        spdlog::warn("ConnectivityArbiter: STA join failed ({}); reverting to P2P", sta.detail);
        last_error_ = sta.detail;
        uplink_.TeardownWifiSta();
        hooks_.OnRevertedToP2p();
        mode_ = ConnectivityMode::kP2pOnly;
        return std::nullopt;
    }
    last_error_.clear();  // this request succeeded — no stale reason for the app

    const MigrationId migration_id = ++migration_counter_;
    mode_ = ConnectivityMode::kMigrating;
    current_id_ = migration_id;
    current_ssid_ =
        uplink.ssid.value_or("");    // the network we're joining (tracked for CurrentSsid)
    preview_endpoint_ = sta.detail;  // the resolved STA LAN IP
    preview_credential_ = hooks_.OnSharedWifiJoined(preview_endpoint_);

    // Backstop only (KTD5): a crashed app that never confirms is caught here; the
    // epoch guard, not this timer, is the correctness mechanism.
    timer_.Arm(revert_after_, [this, migration_id] { OnSelfRevertTimeout(migration_id); });

    spdlog::info("ConnectivityArbiter: migrating id={} sta_ip={} -> {}", migration_id,
                 preview_endpoint_, mode_);
    return SharedWifiMigration{.id = migration_id,
                               .preview_endpoint = preview_endpoint_,
                               .preview_credential = preview_credential_};
}

auto ConnectivityArbiter::ConfirmSharedWifi(MigrationId migration_id,
                                            bool reachable) -> ConnectivityMode {
    std::lock_guard<std::mutex> lock(mtx_);

    // Epoch guard: only the in-flight migration, and only while still awaiting a
    // verdict, may be confirmed. A stale/superseded id — or a verdict that lands
    // after the self-revert already fired — is a no-op.
    if (mode_ != ConnectivityMode::kMigrating || migration_id != current_id_) {
        spdlog::warn("ConnectivityArbiter: confirm ignored id={} reachable={} (mode={} current={})",
                     migration_id, reachable, mode_, current_id_);
        return mode_;
    }

    // Atomic disarm (KTD5): the timer can no longer act on this migration, so the
    // app-confirm and the timer never both transition the same epoch.
    timer_.Disarm();

    if (reachable) {
        mode_ = ConnectivityMode::kStaOnly;
        spdlog::info("ConnectivityArbiter: confirmed id={} -> {}", migration_id, mode_);
        return mode_;
    }

    spdlog::info("ConnectivityArbiter: denied id={} (isolation) -> reverting", migration_id);
    DoRevertLocked();
    return mode_;
}

auto ConnectivityArbiter::RevertToP2p() -> void {
    std::lock_guard<std::mutex> lock(mtx_);
    if (mode_ == ConnectivityMode::kP2pOnly) {
        return;
    }
    spdlog::info("ConnectivityArbiter: RevertToP2p from {}", mode_);
    DoRevertLocked();
}

auto ConnectivityArbiter::DoRevertLocked() -> void {
    timer_.Disarm();
    // Cleanly tear the STA down FIRST (release the lease, kill dhclient, flush the
    // address + routes) so no leftover STA IP/route lingers to fight another
    // interface or dirty the radio — THEN restore the GO. Without this a reverted
    // migration can wedge the camera (e.g. a stale STA address on the same subnet
    // as ethernet breaks its routing).
    uplink_.TeardownWifiSta();
    // Return the radio to the GO. The hook restores the GO preview; a failure is
    // logged but does not keep us in a STA mode — the radio is no longer STA either
    // way.
    if (!hooks_.OnRevertedToP2p()) {
        spdlog::warn("ConnectivityArbiter: GO preview restore reported failure (preview degraded)");
    }
    mode_ = ConnectivityMode::kP2pOnly;
    current_id_ = 0;
    current_ssid_.clear();
    preview_endpoint_.clear();
    preview_credential_.clear();
}

auto ConnectivityArbiter::OnSelfRevertTimeout(MigrationId migration_id) -> void {
    std::lock_guard<std::mutex> lock(mtx_);
    // Only fire if this exact migration is still awaiting confirmation. If the app
    // already confirmed (mode left kMigrating) or a newer migration superseded
    // this id, the late timer is a harmless no-op.
    if (mode_ != ConnectivityMode::kMigrating || migration_id != current_id_) {
        return;
    }
    spdlog::info("ConnectivityArbiter: self-revert timeout id={} (no confirm) -> reverting",
                 migration_id);
    DoRevertLocked();
}

auto ConnectivityArbiter::CurrentMode() const -> ConnectivityMode {
    std::lock_guard<std::mutex> lock(mtx_);
    return mode_;
}

auto ConnectivityArbiter::LastError() const -> std::string {
    std::lock_guard<std::mutex> lock(mtx_);
    return last_error_;
}

auto ConnectivityArbiter::CurrentSsid() const -> std::string {
    std::lock_guard<std::mutex> lock(mtx_);
    return current_ssid_;
}

auto ConnectivityArbiter::CurrentMigrationId() const -> MigrationId {
    std::lock_guard<std::mutex> lock(mtx_);
    return current_id_;
}

auto ConnectivityArbiter::PreviewEndpoint() const -> std::string {
    std::lock_guard<std::mutex> lock(mtx_);
    return preview_endpoint_;
}

auto ConnectivityArbiter::PreviewCredential() const -> std::string {
    std::lock_guard<std::mutex> lock(mtx_);
    return preview_credential_;
}

}  // namespace sst::network
