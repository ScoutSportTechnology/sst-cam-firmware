#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>

#include "app/control/ports/wifi-manager.hpp"
#include "app/network/ports/connectivity-arbiter.hpp"
#include "app/network/ports/ethernet-carrier-monitor.hpp"
#include "app/network/ports/ethernet-controller.hpp"
#include "app/network/ports/policy-routing.hpp"
#include "app/network/ports/self-revert-timer.hpp"
#include "app/network/ports/uplink-configurator.hpp"
#include "domain/config/models/uplink-config.hpp"
#include "domain/network/models/connectivity-mode.hpp"

namespace sst::network {

// Concrete arbiter (KTD3/KTD5 + U4). The SINGLE LAN-path decision site: it owns
// the radio mode (kP2pOnly/kMigrating/kStaOnly, INV2) AND the orthogonal LanPath
// (Ethernet/WiFi-STA/P2P), and is the only object that sequences GO-teardown
// before STA-up. It gains an Ethernet input (IEthernetController) so
// Ethernet-on-the-phone-subnet beats STA beats P2P (R3), with probe-confirm-
// before-suppress (R19) and event-driven re-arbitration via an always-on
// mode-independent Ethernet-carrier monitor (R20).
//
// SINGLE GLOBAL EPOCH (KTD "single-counter epoch"): `migration_counter_`/
// `current_id_` is bumped on EVERY re-arbitration and reused for BOTH the STA
// migration handshake AND the Ethernet-LAN handshake. ConfirmLanPath finalises
// only when confirm_id == current_id_ AND path == current_path_, so a stale
// confirm (wrong id OR wrong path) is a no-op — there is deliberately no second,
// per-path confirm id.
//
// Every public method serialises on `mtx_`; re-entrant events during a transition
// are handled one at a time and never open a concurrent-radio window.
class ConnectivityArbiter final : public IConnectivityArbiter {
   public:
    // `revert_after` is the self-revert backstop window (OQ1): strictly longer
    // than a normal app probe round-trip. Correctness rests on the epoch guard,
    // not this value. `ethernet` is the firmware-owned Ethernet input; `monitor`
    // is the always-on carrier monitor the arbiter Starts here and Stops in its
    // destructor.
    ConnectivityArbiter(sst::control::IWifiManager& wifi, IUplinkConfigurator& uplink,
                        ISelfRevertTimer& timer, IConnectivityTransitionHooks& hooks,
                        IEthernetController& ethernet, IEthernetCarrierMonitor& monitor,
                        IPolicyRouting& policy_routing, std::chrono::milliseconds revert_after);
    ~ConnectivityArbiter() override;

    ConnectivityArbiter(const ConnectivityArbiter&) = delete;
    auto operator=(const ConnectivityArbiter&) -> ConnectivityArbiter& = delete;
    ConnectivityArbiter(ConnectivityArbiter&&) = delete;
    auto operator=(ConnectivityArbiter&&) -> ConnectivityArbiter& = delete;

    // ── IConnectivityArbiter ────────────────────────────────────────────
    auto SelectLanPath(const sst::config::UplinkData& desired,
                       const std::string& phone_subnet_cidr) -> LanSelectionResult override;
    auto ConfirmSharedWifi(MigrationId migration_id, bool reachable) -> ConnectivityMode override;
    auto ConfirmLanPath(MigrationId confirm_id, LanPath path, bool reachable) -> LanPath override;
    auto RevertToP2p() -> void override;
    [[nodiscard]] auto CurrentMode() const -> ConnectivityMode override;
    [[nodiscard]] auto CurrentLanPath() const -> LanPath override;
    [[nodiscard]] auto LastError() const -> std::string override;
    [[nodiscard]] auto CurrentSsid() const -> std::string override;
    [[nodiscard]] auto EthernetEndpoint() const -> std::string override;
    [[nodiscard]] auto EthernetCredential() const -> std::string override;
    [[nodiscard]] auto EthernetConfirmId() const -> MigrationId override;

    // The STA migration primitive. No longer on the port (the handler drives
    // SelectLanPath) but kept public so the arbiter's own tests exercise the STA
    // leg directly. Tear the GO down, join STA, mint the credential, arm the
    // self-revert timer, enter kMigrating. nullopt = a migration was already in
    // flight or the STA join failed (arbiter already reverted to a legal state).
    auto RequestSharedWifi(const sst::config::WifiStaUplink& uplink)
        -> std::optional<SharedWifiMigration>;

    // Accessors for the network command handler (U4/U8) to build
    // NetworkConfigResponse. PreviewEndpoint/PreviewCredential override the port so
    // the handler can surface the committed STA LAN handshake on a plain Get (U8).
    // CurrentMigrationId (F5) surfaces the live STA epoch on a plain Get so an
    // Ethernet->STA fallback migration is not stranded.
    [[nodiscard]] auto CurrentMigrationId() const -> MigrationId override;
    [[nodiscard]] auto PreviewEndpoint() const -> std::string override;
    [[nodiscard]] auto PreviewCredential() const -> std::string override;

   private:
    // How a co-present Ethernet interface relates to the phone's subnet (R3/R21).
    enum class EthernetRole : std::uint8_t {
        kNotACandidate,  // disabled, carrier down, or misconfigured static
        kDhcpPending,    // carrier up, DHCP, no lease/offer yet (R21 — not yet a candidate)
        kLanSameSubnet,  // on the phone's subnet -> the LAN path
        kUplinkDifferentSubnet,  // a different subnet -> an internet uplink (LAN falls to STA/P2P;
                                 // U7)
    };
    struct EthernetClassification {
        EthernetRole role{EthernetRole::kNotACandidate};
        std::string cidr;  // the candidate address (bound, offered, or configured static)
    };

    // Everything the HEAVY (potentially 20s dhclient) Ethernet interaction resolves
    // OFF the lock (F3), handed to the locked commit so no shell-out runs under
    // `mtx_` on the carrier-monitor path (which would freeze every BLE command).
    struct PreparedEthernet {
        EthernetProbe eth;
        std::optional<EthernetClassification> classification;
        // A different-subnet uplink has NO STA ordering constraint, so its address is
        // bound LOCKLESS here (real ApplyEthernet — a lease/static bind, not the
        // no-bind ProbeDhcpOffer, F1) and the routing installed against the bound
        // address in the locked commit — never a dhclient under `mtx_`.
        bool uplink_bound{false};
        bool uplink_bind_ok{false};
        EthernetProbe uplink_probe;  // re-probe after the bind (real address + gateway)
        std::string uplink_cidr;     // the bound uplink CIDR for UplinkRoutingConfig
    };

    // Resolve the Ethernet decision WITHOUT holding `mtx_` (F3): ProbeEthernet +
    // (DHCP) ProbeDhcpOffer classification, plus a different-subnet uplink bind.
    // `sta_ip` is the snapshotted STA LAN IP for ground-truth collision (R2/A4).
    auto PrepareEthernetTransition(const sst::config::UplinkData& desired,
                                   const std::string& phone_subnet_cidr,
                                   const std::string& sta_ip) -> PreparedEthernet;

    // The core decision (R3), caller holds `mtx_`. `unsolicited` = driven by the
    // carrier monitor (a hotplug/unplug), NOT an operator SetNetworkConfig — it
    // gates R2>R19 immediate suppression + the R21 no-surprise-migrate guard.
    // `prepared` carries the off-lock Ethernet resolution (F3); when engaged the
    // locked path never runs a dhclient under `mtx_`.
    auto ArbitrateLocked(bool unsolicited, const PreparedEthernet& prepared) -> LanSelectionResult;
    // Classify the Ethernet interface against the phone subnet (acquire-without-bind
    // for DHCP). Lock-free: reads only its arguments + the Ethernet controller, so it
    // runs in the off-lock prepare phase. `sta_ip` is the live STA LAN IP.
    auto ClassifyEthernet(const EthernetProbe& eth, const sst::config::UplinkData& desired,
                          const std::string& phone_subnet_cidr,
                          const std::string& sta_ip) -> EthernetClassification;
    // U7: apply (or tear down) the different-subnet uplink policy routing +
    // egress-only firewall to match the classification. `want` == Ethernet is a
    // different-subnet uplink; `cidr`/`eth` supply the uplink address + gateway.
    // Idempotent — the adapter converges from any prior state. Caller holds `mtx_`.
    auto SyncUplinkRoutingLocked(bool want, const EthernetProbe& eth,
                                 const std::string& cidr) -> void;
    // Commit (or begin committing) Ethernet as the LAN path. Caller holds `mtx_`.
    auto SelectEthernetLanLocked(const std::string& cidr, bool unsolicited) -> LanSelectionResult;
    // Route the LAN path to WiFi-STA (wraps RequestSharedWifi). Caller holds `mtx_`.
    // `unsolicited` re-arbitration that resolves to "STA stays" keeps a HEALTHY
    // kStaOnly session instead of tearing it down and re-migrating.
    auto SelectStaLocked(bool unsolicited) -> LanSelectionResult;
    // Settle the LAN path on P2P (idempotent). Caller holds `mtx_`.
    auto SelectP2pLocked() -> LanSelectionResult;
    // Tear a de-selected/committed Ethernet LAN path down + clear its handshake.
    // Caller holds `mtx_`.
    auto TeardownEthernetLanLocked() -> void;
    // True iff `sta_ip` falls inside the Ethernet candidate's real subnet
    // (ground-truth reconciliation, R2/A4). Pure (reads only its arguments) so it is
    // static and runs in the off-lock classify phase.
    // The host IP and network CIDR are shape-validated before comparison.
    static auto StaCollidesWith(const std::string& sta_ip,
                                const std::string& ethernet_cidr) -> bool;

    // The STA migration primitive body (caller holds `mtx_`).
    auto RequestSharedWifiLocked(const sst::config::WifiStaUplink& uplink)
        -> std::optional<SharedWifiMigration>;
    auto ConfirmSharedWifiLocked(MigrationId migration_id, bool reachable) -> ConnectivityMode;

    // Returns the radio to the GO and resets migration state. Caller holds `mtx_`.
    auto DoRevertLocked() -> void;
    // Self-revert timer expiry — no-op unless still kMigrating on the same epoch.
    auto OnSelfRevertTimeout(MigrationId migration_id) -> void;
    // Arm the S2 carrier watchdog (reuses `timer_`, which is idle in kStaOnly).
    // Caller holds `mtx_`.
    auto ArmCarrierWatchdogLocked() -> void;
    // Periodic S2 carrier check (D2a). While kStaOnly, probes the STA link; on a
    // sustained loss it auto-reverts to P2P so live preview resumes over the GO
    // even if BLE is also down (removes the single-point app dependency). Runs on
    // the timer thread; probes WITHOUT `mtx_` held (the probe shells wpa_cli).
    auto OnCarrierCheck() -> void;
    // Always-on Ethernet-carrier monitor callback (R20). Probes the carrier WITHOUT
    // `mtx_` (off the dispatcher), then re-arbitrates ONLY on an actual up/down
    // transition — regardless of radio mode.
    auto OnEthernetCarrierPoll() -> void;

    sst::control::IWifiManager& wifi_;
    IUplinkConfigurator& uplink_;
    ISelfRevertTimer& timer_;
    IConnectivityTransitionHooks& hooks_;
    IEthernetController& ethernet_;
    IEthernetCarrierMonitor& eth_monitor_;
    IPolicyRouting& policy_routing_;
    const std::chrono::milliseconds revert_after_;

    mutable std::mutex mtx_;
    ConnectivityMode mode_{ConnectivityMode::kP2pOnly};  // R-BOOT: boot starts in S1
    // Boot serves the phone over the WiFi-Direct GO, so the initial LAN path is P2P
    // (concretised on the first SelectLanPath / carrier event).
    LanPath current_path_{LanPath::kP2p};
    MigrationId migration_counter_{0};
    MigrationId current_id_{0};
    std::string preview_endpoint_;
    std::string preview_credential_;
    std::string current_ssid_;    // SSID being joined / connected to (empty in kP2pOnly)
    std::string last_error_;      // reason the last request failed (empty on success)
    int carrier_loss_streak_{0};  // consecutive S2 carrier-check failures (D2a watchdog)

    // The desired uplink + phone subnet from the last SelectLanPath — remembered so
    // the carrier monitor can re-run arbitration on an Ethernet event without the
    // handler.
    sst::config::UplinkData desired_uplink_;
    std::string phone_subnet_cidr_;
    // The Ethernet-LAN handshake currently surfaced (endpoint = Ethernet IP,
    // credential from the hook, confirm-id == current_id_). Empty when Ethernet is
    // not the committing/committed LAN path.
    std::string ethernet_endpoint_;
    std::string ethernet_credential_;
    // Last observed Ethernet carrier, so OnEthernetCarrierPoll de-dupes and only
    // re-arbitrates on a real up/down transition.
    bool ethernet_carrier_{false};
    // U7: whether the different-subnet uplink policy routing + firewall APPLY last
    // reported full success. Reported to callers/logs; NOT the teardown gate (F6).
    bool uplink_routing_active_{false};
    // F6: whether ApplyUplinkRouting has been INVOKED (touched the table/rule/firewall)
    // since the last teardown — SEPARATE from "apply fully succeeded". A partial apply
    // still creates the nft drop table, so teardown-on-transition-away gates on this,
    // never on the success flag, so a leaked drop table is always cleaned up.
    bool uplink_routing_touched_{false};
    // F4: the arbiter is holding a DHCP-pending Ethernet candidate (carrier up, no
    // lease/offer yet). The carrier monitor re-arbitrates on THIS flag (not only a
    // carrier edge) so a lease that lands later — with no carrier transition — still
    // promotes Ethernet. Cleared once the candidate resolves (leases / becomes an
    // uplink / carrier drops / operator overrides).
    bool dhcp_pending_{false};
};

}  // namespace sst::network
