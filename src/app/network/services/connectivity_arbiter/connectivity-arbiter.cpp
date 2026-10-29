#include "app/network/services/connectivity_arbiter/connectivity-arbiter.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include "domain/network/models/formatter/_fmt.hpp"  // IWYU pragma: keep

namespace sst::network {

namespace {
// S2 carrier watchdog (D2a): poll the STA link this often, and auto-revert to
// P2P after this many CONSECUTIVE lost-carrier checks (interval * strikes ~=
// the grace before recovery). Kept coarse — this is a stranding backstop for the
// both-links-down case, not a fast failover.
constexpr auto kCarrierCheckInterval = std::chrono::seconds{5};
constexpr int kCarrierLossStrikes = 3;

// IPv4 CIDR math for same-vs-different-subnet classification (R3/R2). Pure
// parsing — no shell, no allocation of untrusted argv (injection-safe).
constexpr int kIpv4Bits = 32;
constexpr int kOctetBits = 8;
constexpr int kOctetCount = 4;
constexpr int kOctetMax = 255;
constexpr int kDecimalBase = 10;
constexpr std::uint32_t kFullMask = 0xFFFFFFFFU;

// A parsed IPv4 host + prefix length (no prefix => host route /32).
struct Ipv4Subnet {
    std::uint32_t address{0};
    int prefix{kIpv4Bits};
};

// The bare address of a CIDR ("10.10.1.30/24" -> "10.10.1.30"; a bare IP is
// returned unchanged).
auto AddressOnly(const std::string& cidr) -> std::string {
    const auto slash = cidr.find('/');
    return slash == std::string::npos ? cidr : cidr.substr(0, slash);
}

// Parse dotted-quad "a.b.c.d" -> packed uint32, or nullopt on any malformation
// (wrong octet count, >255, non-numeric, trailing junk).
auto ParseIpv4(const std::string& text) -> std::optional<std::uint32_t> {
    std::uint32_t packed = 0;
    std::size_t pos = 0;
    for (int octet = 0; octet < kOctetCount; ++octet) {
        if (pos >= text.size() || std::isdigit(static_cast<unsigned char>(text[pos])) == 0) {
            return std::nullopt;
        }
        int value = 0;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
            value = (value * kDecimalBase) + (text[pos] - '0');
            ++pos;
            if (value > kOctetMax) {
                return std::nullopt;
            }
        }
        packed = (packed << kOctetBits) | static_cast<std::uint32_t>(value);
        if (octet < kOctetCount - 1) {
            if (pos >= text.size() || text[pos] != '.') {
                return std::nullopt;
            }
            ++pos;
        }
    }
    return pos == text.size() ? std::optional<std::uint32_t>{packed} : std::nullopt;
}

// Parse "a.b.c.d[/p]" -> address + prefix, or nullopt on malformation.
auto ParseCidr(const std::string& cidr) -> std::optional<Ipv4Subnet> {
    const auto slash = cidr.find('/');
    const auto address = ParseIpv4(slash == std::string::npos ? cidr : cidr.substr(0, slash));
    if (!address) {
        return std::nullopt;
    }
    int prefix = kIpv4Bits;
    if (slash != std::string::npos) {
        const std::string digits = cidr.substr(slash + 1);
        if (digits.empty()) {
            return std::nullopt;
        }
        prefix = 0;
        for (const char digit : digits) {
            if (std::isdigit(static_cast<unsigned char>(digit)) == 0) {
                return std::nullopt;
            }
            prefix = (prefix * kDecimalBase) + (digit - '0');
            if (prefix > kIpv4Bits) {
                return std::nullopt;
            }
        }
    }
    return Ipv4Subnet{.address = *address, .prefix = prefix};
}

// The netmask for a prefix length (prefix 0 -> 0.0.0.0, avoiding a UB 32-bit
// shift-by-32).
auto MaskForPrefix(int prefix) -> std::uint32_t {
    return prefix == 0 ? 0U : (kFullMask << (kIpv4Bits - prefix));
}

// True iff `lhs_cidr` and `rhs_cidr` share a network under the MORE INCLUSIVE
// (smaller) of their two prefixes — i.e. one covers the other's subnet. A
// malformed CIDR is never "same subnet" (fail closed).
auto SameSubnet(const std::string& lhs_cidr, const std::string& rhs_cidr) -> bool {
    const auto lhs = ParseCidr(lhs_cidr);
    const auto rhs = ParseCidr(rhs_cidr);
    if (!lhs || !rhs) {
        return false;
    }
    const std::uint32_t mask = MaskForPrefix(std::min(lhs->prefix, rhs->prefix));
    return (lhs->address & mask) == (rhs->address & mask);
}

// True iff `host_ip` falls inside `network_cidr`'s subnet (ground-truth
// reconciliation: STA's real IP inside Ethernet's real subnet). Fail closed on a
// malformed input.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) // floor-ok: CIDR vs host IP
auto NetworkContains(const std::string& network_cidr, const std::string& host_ip) -> bool {
    const auto network = ParseCidr(network_cidr);
    const auto host = ParseIpv4(AddressOnly(host_ip));
    if (!network || !host) {
        return false;
    }
    const std::uint32_t mask = MaskForPrefix(network->prefix);
    return (network->address & mask) == (*host & mask);
}
}  // namespace

ConnectivityArbiter::ConnectivityArbiter(sst::control::IWifiManager& wifi,
                                         IUplinkConfigurator& uplink, ISelfRevertTimer& timer,
                                         IConnectivityTransitionHooks& hooks,
                                         IEthernetController& ethernet,
                                         IEthernetCarrierMonitor& monitor,
                                         IPolicyRouting& policy_routing,
                                         std::chrono::milliseconds revert_after)
    : wifi_(wifi),
      uplink_(uplink),
      timer_(timer),
      hooks_(hooks),
      ethernet_(ethernet),
      eth_monitor_(monitor),
      policy_routing_(policy_routing),
      revert_after_(revert_after) {
    // R-BOOT: the arbiter is born in kP2pOnly (LAN path P2P over the GO) and never
    // auto-applies a stored uplink — a shared LAN is only ever reached through an
    // app-confirmed selection.
    spdlog::info("ConnectivityArbiter: boot mode={} lan_path={}", mode_, current_path_);
    // R20: the always-on carrier monitor runs for the whole process lifetime, so an
    // Ethernet hotplug in kP2pOnly (or an unplug while LanPath=kEthernet) re-runs
    // arbitration regardless of the radio mode. The callback probes off the BLE
    // dispatcher; the arbiter de-dupes on the observed carrier.
    eth_monitor_.Start([this] { OnEthernetCarrierPoll(); });
}

ConnectivityArbiter::~ConnectivityArbiter() {
    // Stop the monitor FIRST so no carrier poll fires into a half-torn-down arbiter.
    eth_monitor_.Stop();
    // U7: leave the routing table clean on shutdown so stale uplink rules/firewall
    // don't linger past the process (a fresh boot re-applies idempotently anyway).
    // F6: gate on TOUCHED (a partial apply may have left a drop table with active_=
    // false), never on the success flag.
    if (uplink_routing_touched_) {
        policy_routing_.TeardownUplinkRouting();
        uplink_routing_active_ = false;
        uplink_routing_touched_ = false;
    }
}

auto ConnectivityArbiter::SelectLanPath(const sst::config::UplinkData& desired,
                                        const std::string& phone_subnet_cidr)
    -> LanSelectionResult {
    std::string sta_ip;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        // Remember the operator's intent so the carrier monitor can re-arbitrate on an
        // Ethernet event without the handler.
        desired_uplink_ = desired;
        phone_subnet_cidr_ = phone_subnet_cidr;
        sta_ip = preview_endpoint_;
        spdlog::info(
            "ConnectivityArbiter: SelectLanPath eth_enabled={} wifi_enabled={} phone_subnet='{}'",
            desired.ethernet.enabled.value_or(false), desired.wifi.enabled.value_or(false),
            phone_subnet_cidr);
    }
    // F3: resolve the Ethernet decision (probe + DHCP offer + different-subnet uplink
    // bind — all potentially 20s dhclient calls) WITHOUT holding `mtx_`, so a slow
    // shell-out never serializes behind the BLE command mutex.
    const PreparedEthernet prepared = PrepareEthernetTransition(desired, phone_subnet_cidr, sta_ip);
    std::lock_guard<std::mutex> lock(mtx_);
    return ArbitrateLocked(/*unsolicited=*/false, prepared);
}

auto ConnectivityArbiter::PrepareEthernetTransition(const sst::config::UplinkData& desired,
                                                    const std::string& phone_subnet_cidr,
                                                    const std::string& sta_ip) -> PreparedEthernet {
    PreparedEthernet prepared;
    prepared.eth = ethernet_.ProbeEthernet();  // off-lock (F3)
    if (desired.ethernet.enabled.value_or(false) && prepared.eth.carrier) {
        // ClassifyEthernet may ProbeDhcpOffer (a 20s dhclient) — off the lock (F3).
        prepared.classification =
            ClassifyEthernet(prepared.eth, desired, phone_subnet_cidr, sta_ip);
    }
    // F1 + F3: a different-subnet uplink has NO STA ordering constraint, so ADDRESS it
    // here off-lock (a REAL ApplyEthernet lease/static bind, not the no-bind offer) and
    // re-probe for the routing config — the arbiter installs routing against the bound
    // address without ever running a dhclient under `mtx_`.
    if (prepared.classification &&
        prepared.classification->role == EthernetRole::kUplinkDifferentSubnet) {
        const auto applied = ethernet_.ApplyEthernet(desired.ethernet);
        prepared.uplink_bound = true;
        prepared.uplink_bind_ok = applied.ok;
        if (applied.ok) {
            prepared.uplink_probe = ethernet_.ProbeEthernet();
            prepared.uplink_cidr = !prepared.uplink_probe.address.empty()
                                       ? prepared.uplink_probe.address
                                       : applied.detail;
        }
    }
    return prepared;
}

auto ConnectivityArbiter::ArbitrateLocked(bool unsolicited,
                                          const PreparedEthernet& prepared) -> LanSelectionResult {
    ethernet_carrier_ = prepared.eth.carrier;
    const auto& classification = prepared.classification;

    // F4: remember a DHCP-pending candidate so the carrier monitor re-polls the offer
    // even without a carrier edge (a lease that lands later still promotes Ethernet).
    dhcp_pending_ = classification && classification->role == EthernetRole::kDhcpPending;

    // U7 (R4): the ONE legit concurrent dual-interface case — Ethernet on a DIFFERENT
    // subnet as an internet uplink — gets source-based policy routing + an egress-only
    // firewall; EVERY other outcome tears that down.
    const bool want_uplink_routing =
        classification && classification->role == EthernetRole::kUplinkDifferentSubnet;
    if (want_uplink_routing) {
        // F1: the uplink address was already bound off-lock (PrepareEthernetTransition).
        // A same-subnet-LAN -> different-subnet-uplink transition must drop the stale
        // kEthernet LAN handshake WITHOUT flushing (the bound uplink address stays).
        if (current_path_ == LanPath::kEthernet) {
            ethernet_endpoint_.clear();
            ethernet_credential_.clear();
            current_id_ = 0;
            current_path_ = LanPath::kNone;
        }
        if (prepared.uplink_bound && prepared.uplink_bind_ok) {
            SyncUplinkRoutingLocked(/*want=*/true, prepared.uplink_probe, prepared.uplink_cidr);
        } else {
            // The uplink could not be addressed — do NOT install routing against an
            // unbound address; tear down any stale uplink table (F6).
            spdlog::warn(
                "ConnectivityArbiter: different-subnet uplink not addressed — no uplink egress");
            SyncUplinkRoutingLocked(/*want=*/false, prepared.eth, {});
        }
    } else {
        // Any non-uplink outcome tears down a prior uplink routing exactly once (F6:
        // gated on TOUCHED, not the success flag).
        SyncUplinkRoutingLocked(/*want=*/false, prepared.eth, {});
    }

    if (classification) {
        if (classification->role == EthernetRole::kLanSameSubnet) {
            return SelectEthernetLanLocked(classification->cidr, unsolicited);
        }
        if (classification->role == EthernetRole::kDhcpPending) {
            // R21: carrier up, no lease yet — not yet a candidate. Fall to STA/P2P;
            // a later lease re-arbitrates via the carrier monitor (F4 pending flag).
            spdlog::info("ConnectivityArbiter: Ethernet DHCP-pending — not yet a LAN candidate");
        } else if (classification->role == EthernetRole::kUplinkDifferentSubnet) {
            // R4: a different-subnet Ethernet is an internet uplink, NOT a LAN path.
            // It is bound + policy-routed (above); LAN falls to STA/P2P.
            spdlog::info(
                "ConnectivityArbiter: Ethernet on a different subnet — bound + uplink policy "
                "routing applied (U7/F1), LAN falls to STA/P2P");
        }
    }

    // Ethernet is not (or no longer) the LAN path: tear down any committed Ethernet
    // address before falling through, so a stale same-subnet address can't linger.
    if (current_path_ == LanPath::kEthernet) {
        TeardownEthernetLanLocked();
    }
    if (desired_uplink_.wifi.enabled.value_or(false)) {
        return SelectStaLocked(unsolicited);
    }
    return SelectP2pLocked();
}

auto ConnectivityArbiter::SyncUplinkRoutingLocked(bool want, const EthernetProbe& eth,
                                                  const std::string& cidr) -> void {
    if (want) {
        // F6: mark the table TOUCHED before the apply — a partial apply still creates
        // the nft drop table, so a later transition-away must tear it down even if the
        // apply reported failure.
        uplink_routing_touched_ = true;
        // Idempotent apply (the adapter swaps atomically, F7). `cidr` is the uplink's
        // bound address WITH prefix; the gateway comes from the real Ethernet probe
        // (empty => no default route, only the source rule + connected route). A failed
        // apply leaves STA as the LAN with no uplink egress.
        const UplinkRoutingConfig cfg{.uplink_address_cidr = cidr, .uplink_gateway = eth.gateway};
        uplink_routing_active_ = policy_routing_.ApplyUplinkRouting(cfg);
        if (!uplink_routing_active_) {
            spdlog::warn(
                "ConnectivityArbiter: uplink policy routing apply failed — STA remains the LAN, no "
                "uplink egress (the drop table is torn down on the next transition, F6)");
        }
        return;
    }
    // F6: tear down on ANY transition-away when the table was ever TOUCHED — NEVER
    // gated on the success flag. The old gate (`if uplink_routing_active_`) skipped a
    // partially-applied apply (active_=false) and leaked its drop table.
    if (uplink_routing_touched_) {
        policy_routing_.TeardownUplinkRouting();
        uplink_routing_active_ = false;
        uplink_routing_touched_ = false;
    }
}

// phone_subnet_cidr / sta_ip are distinct roles (network CIDR vs host IP), each
// shape-validated; a swap fails the subnet checks.
auto ConnectivityArbiter::ClassifyEthernet(
    const EthernetProbe& eth, const sst::config::UplinkData& desired,
    const std::string&
        phone_subnet_cidr,  // NOLINT(bugprone-easily-swappable-parameters) // floor-ok: CIDR vs IP
    const std::string& sta_ip) -> EthernetClassification {
    const bool is_dhcp = desired.ethernet.dhcp.value_or(true);
    std::string cidr;
    if (is_dhcp) {
        if (eth.mode == EthernetMode::kDhcp && !eth.address.empty()) {
            cidr = eth.address;  // an already-bound lease
        } else {
            // A2 window: acquire the OFFER without binding, so a same-subnet lease is
            // classified BEFORE any address touches the interface.
            const auto offer = ethernet_.ProbeDhcpOffer();
            if (!offer || offer->empty()) {
                return {.role = EthernetRole::kDhcpPending, .cidr = {}};
            }
            cidr = *offer;
        }
    } else {
        cidr = desired.ethernet.address.value_or("");
        if (cidr.empty()) {
            return {.role = EthernetRole::kNotACandidate, .cidr = {}};
        }
    }
    if (phone_subnet_cidr.empty()) {
        // No phone Wi-Fi to collide with -> Ethernet is trivially the LAN path.
        return {.role = EthernetRole::kLanSameSubnet, .cidr = cidr};
    }
    bool same = SameSubnet(cidr, phone_subnet_cidr);
    if (!same && StaCollidesWith(sta_ip, cidr)) {
        // Ground-truth reconciliation (R2/A4): the phone CIDR claimed different, but
        // STA's REAL address is inside Ethernet's REAL subnet — refuse the R4
        // dual-config and treat this as same-subnet (single-path).
        spdlog::warn(
            "ConnectivityArbiter: phone CIDR said different-subnet but STA collides with "
            "Ethernet's real subnet — refusing dual-home (R2/A4)");
        same = true;
    }
    return {.role = same ? EthernetRole::kLanSameSubnet : EthernetRole::kUplinkDifferentSubnet,
            .cidr = cidr};
}

auto ConnectivityArbiter::SelectEthernetLanLocked(const std::string& cidr,
                                                  bool unsolicited) -> LanSelectionResult {
    const bool sta_live =
        mode_ == ConnectivityMode::kStaOnly || mode_ == ConnectivityMode::kMigrating;
    const bool is_dhcp = desired_uplink_.ethernet.dhcp.value_or(true);

    // R21 no-surprise-migrate: a LATE same-subnet DHCP lease must NOT auto-migrate a
    // HEALTHY STA session (a preview drop ~30s after the cable was plugged, with no
    // operator action). The offer was classified WITHOUT binding, so leaving STA up
    // dual-homes nothing. Only an operator SelectLanPath (solicited) switches.
    if (sta_live && unsolicited && is_dhcp) {
        spdlog::info(
            "ConnectivityArbiter: late same-subnet DHCP Ethernet lease — keeping healthy STA (no "
            "surprise migrate, R21)");
        return {.ok = true,
                .error = {},
                .path = LanPath::kWifiSta,
                .confirm_id = 0,
                .endpoint = preview_endpoint_,
                .credential = preview_credential_};
    }

    if (sta_live) {
        // Same-subnet: the two interfaces CANNOT coexist (R2). Suppress the STA IP
        // FIRST (tear it down, restore the GO), THEN bind Ethernet — no transient
        // dual-home. For an UNSOLICITED static hotplug this is the documented R2>R19
        // precedence (suppress-before-confirm to prevent the guaranteed collision);
        // for an operator switch the operator explicitly chose it.
        spdlog::info(
            "ConnectivityArbiter: Ethernet same-subnet — suppressing STA before binding Ethernet "
            "(R2>R19, unsolicited={})",
            unsolicited);
        DoRevertLocked();
    }

    // No STA IP now (the P2P GO on 192.168.49.x never collides). Bind Ethernet.
    const auto applied = ethernet_.ApplyEthernet(desired_uplink_.ethernet);
    if (!applied.ok) {
        spdlog::warn("ConnectivityArbiter: Ethernet apply failed ({}) — falling to STA/P2P",
                     applied.detail);
        last_error_ = applied.detail;
        TeardownEthernetLanLocked();
        return desired_uplink_.wifi.enabled.value_or(false) ? SelectStaLocked(/*unsolicited=*/false)
                                                            : SelectP2pLocked();
    }
    last_error_.clear();

    // F2/F6: Ethernet is now the same-subnet LAN — the camera MUST be reachable on it,
    // so remove ANY egress firewall / uplink routing before committing: the boot
    // egress-only firewall (installed outside the arbiter, F2) OR a leaked partial
    // uplink drop table (F6). Idempotent and UNCONDITIONAL — never gated on a success
    // flag — so a stale drop table can never keep the LAN iface unreachable.
    policy_routing_.TeardownUplinkRouting();
    uplink_routing_active_ = false;
    uplink_routing_touched_ = false;

    const MigrationId epoch = ++migration_counter_;  // SINGLE global epoch (KTD)
    current_id_ = epoch;
    current_path_ = LanPath::kEthernet;
    ethernet_endpoint_ = AddressOnly(applied.detail.empty() ? cidr : applied.detail);
    // U6 fills this hook (mint the per-path credential + rebind the RTSP preview
    // onto the Ethernet IP). U4 gets the endpoint + shared-epoch confirm id; an
    // empty credential here is a correct minimal body, not a stub.
    ethernet_credential_ = hooks_.OnEthernetLanSelected(ethernet_endpoint_);

    // The "BLE notification" for an unsolicited hotplug is realized under the proto's
    // PULL model: lan_path=kEthernet + a fresh ethernet_confirm_id are surfaced on
    // the app's next Get, which triggers its re-probe. There is no unsolicited push
    // channel (proto invariant), so state-the-next-Get-reflects IS the signal.
    spdlog::info("ConnectivityArbiter: Ethernet LAN selected id={} endpoint={} unsolicited={}",
                 epoch, ethernet_endpoint_, unsolicited);
    return {.ok = true,
            .error = {},
            .path = LanPath::kEthernet,
            .confirm_id = epoch,
            .endpoint = ethernet_endpoint_,
            .credential = ethernet_credential_};
}

auto ConnectivityArbiter::SelectStaLocked(bool unsolicited) -> LanSelectionResult {
    // Event-driven re-arbitration that resolved to "STA stays" (e.g. a different-
    // subnet Ethernet appeared, or a late DHCP lease we won't auto-migrate to): keep
    // the HEALTHY kStaOnly session — never tear it down and re-migrate (R21
    // no-surprise-drop). An operator (solicited) SelectLanPath always goes through
    // RequestSharedWifi so a same-SSID-different-credential re-provision is honoured.
    if (unsolicited && mode_ == ConnectivityMode::kStaOnly) {
        current_path_ = LanPath::kWifiSta;
        return {.ok = true,
                .error = {},
                .path = LanPath::kWifiSta,
                .confirm_id = 0,
                .endpoint = preview_endpoint_,
                .credential = preview_credential_};
    }
    const auto migration = RequestSharedWifiLocked(desired_uplink_.wifi);
    if (!migration) {
        // STA join failed OR a migration was already in flight (RequestSharedWifi
        // reverted to a legal state / set last_error_). Report the reason so the
        // handler surfaces ERROR rather than a false green.
        current_path_ =
            (mode_ == ConnectivityMode::kStaOnly || mode_ == ConnectivityMode::kMigrating)
                ? LanPath::kWifiSta
                : LanPath::kP2p;
        return {.ok = false,
                .error = last_error_,
                .path = current_path_,
                .confirm_id = 0,
                .endpoint = {},
                .credential = {}};
    }
    current_path_ = LanPath::kWifiSta;
    return {.ok = true,
            .error = {},
            .path = LanPath::kWifiSta,
            .confirm_id = migration->id,
            .endpoint = migration->preview_endpoint,
            .credential = migration->preview_credential};
}

auto ConnectivityArbiter::SelectP2pLocked() -> LanSelectionResult {
    if (mode_ != ConnectivityMode::kP2pOnly) {
        DoRevertLocked();  // returns the radio to the GO
    }
    current_path_ = LanPath::kP2p;
    return {.ok = true,
            .error = {},
            .path = LanPath::kP2p,
            .confirm_id = 0,
            .endpoint = {},
            .credential = {}};
}

auto ConnectivityArbiter::TeardownEthernetLanLocked() -> void {
    ethernet_.TeardownEthernet();
    ethernet_endpoint_.clear();
    ethernet_credential_.clear();
    if (current_path_ == LanPath::kEthernet) {
        current_path_ = LanPath::kNone;  // caller re-selects STA/P2P next
    }
}

auto ConnectivityArbiter::StaCollidesWith(const std::string& sta_ip,
                                          const std::string& ethernet_cidr) -> bool {
    // STA's real LAN IP is the tracked preview endpoint (set on migrate). Empty ->
    // no STA up -> no collision.
    if (sta_ip.empty()) {
        return false;
    }
    return NetworkContains(ethernet_cidr, sta_ip);
}

auto ConnectivityArbiter::RequestSharedWifi(const sst::config::WifiStaUplink& uplink)
    -> std::optional<SharedWifiMigration> {
    std::lock_guard<std::mutex> lock(mtx_);
    return RequestSharedWifiLocked(uplink);
}

auto ConnectivityArbiter::RequestSharedWifiLocked(const sst::config::WifiStaUplink& uplink)
    -> std::optional<SharedWifiMigration> {
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
        // reason (surfaced to the app as a friendly message + error_message).
        spdlog::warn("ConnectivityArbiter: STA join failed ({}); reverting to P2P", sta.detail);
        last_error_ = sta.detail;
        uplink_.TeardownWifiSta();
        hooks_.OnRevertedToP2p();
        mode_ = ConnectivityMode::kP2pOnly;
        current_path_ = LanPath::kP2p;
        return std::nullopt;
    }
    last_error_.clear();  // this request succeeded — no stale reason for the app

    const MigrationId migration_id = ++migration_counter_;  // SINGLE global epoch (KTD)
    mode_ = ConnectivityMode::kMigrating;
    current_id_ = migration_id;
    current_path_ = LanPath::kWifiSta;
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
    return ConfirmSharedWifiLocked(migration_id, reachable);
}

auto ConnectivityArbiter::ConfirmSharedWifiLocked(MigrationId migration_id,
                                                  bool reachable) -> ConnectivityMode {
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
        current_path_ = LanPath::kWifiSta;
        // D2a: the migration self-revert timer is now disarmed (above); repurpose
        // the same timer as an S2 carrier watchdog so a later carrier loss with a
        // dead BLE link still recovers to P2P instead of stranding the camera.
        carrier_loss_streak_ = 0;
        ArmCarrierWatchdogLocked();
        spdlog::info("ConnectivityArbiter: confirmed id={} -> {}", migration_id, mode_);
        return mode_;
    }

    spdlog::info("ConnectivityArbiter: denied id={} (isolation) -> reverting", migration_id);
    DoRevertLocked();
    return mode_;
}

auto ConnectivityArbiter::ConfirmLanPath(MigrationId confirm_id, LanPath path,
                                         bool reachable) -> LanPath {
    std::lock_guard<std::mutex> lock(mtx_);
    // SINGLE-COUNTER epoch + path guard: finalize ONLY the live selection. A stale
    // confirm (wrong id OR wrong path) is a no-op — this is what stops a
    // select-Ethernet -> flap-to-STA -> re-select-Ethernet sequence from letting the
    // first Ethernet confirm finalize a superseded pick (the counter advanced).
    if (confirm_id != current_id_ || path != current_path_) {
        spdlog::warn(
            "ConnectivityArbiter: ConfirmLanPath ignored id={} path={} reachable={} (live id={} "
            "path={})",
            confirm_id, path, reachable, current_id_, current_path_);
        return current_path_;
    }

    if (path == LanPath::kWifiSta) {
        // Delegate to the STA finalize (one epoch machine) and translate the settled
        // radio mode back to a LAN path.
        const auto mode = ConfirmSharedWifiLocked(confirm_id, reachable);
        current_path_ = mode == ConnectivityMode::kStaOnly ? LanPath::kWifiSta : LanPath::kP2p;
        return current_path_;
    }

    // path == kEthernet.
    if (reachable) {
        // Commit. Any losing STA alternate was already suppressed (same-subnet) or
        // was never brought up (no live STA) — nothing more to suppress.
        spdlog::info("ConnectivityArbiter: Ethernet LAN confirmed id={} -> committed {}",
                     confirm_id, current_path_);
        return current_path_;
    }
    // De-select (R19): tear Ethernet down, retire the epoch, and fall to the next
    // provider (STA -> P2P), re-arbitrating.
    spdlog::info(
        "ConnectivityArbiter: Ethernet LAN de-selected id={} (probe failed) -> falling to next "
        "provider (R19)",
        confirm_id);
    TeardownEthernetLanLocked();
    current_id_ = 0;
    if (desired_uplink_.wifi.enabled.value_or(false)) {
        SelectStaLocked(/*unsolicited=*/false);
    } else {
        SelectP2pLocked();
    }
    return current_path_;
}

auto ConnectivityArbiter::RevertToP2p() -> void {
    std::lock_guard<std::mutex> lock(mtx_);
    if (mode_ == ConnectivityMode::kP2pOnly && current_path_ != LanPath::kEthernet) {
        current_path_ = LanPath::kP2p;
        return;
    }
    spdlog::info("ConnectivityArbiter: RevertToP2p from mode={} path={}", mode_, current_path_);
    if (current_path_ == LanPath::kEthernet) {
        TeardownEthernetLanLocked();
    }
    if (mode_ != ConnectivityMode::kP2pOnly) {
        DoRevertLocked();
    }
    current_path_ = LanPath::kP2p;
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
    carrier_loss_streak_ = 0;
    // F4: a genuine revert to P2P retires any DHCP-pending candidate — a subsequent
    // arbitration recomputes it. (The pending-fell-to-STA case does NOT DoRevert, so
    // its flag correctly persists for the carrier monitor to re-poll.)
    dhcp_pending_ = false;
    preview_endpoint_.clear();
    preview_credential_.clear();
    // Radio-only revert: the LAN path becomes P2P unless the caller is mid-way to
    // committing Ethernet (which overwrites current_path_ right after).
    current_path_ = LanPath::kP2p;
}

auto ConnectivityArbiter::ArmCarrierWatchdogLocked() -> void {
    timer_.Arm(kCarrierCheckInterval, [this] { OnCarrierCheck(); });
}

auto ConnectivityArbiter::OnCarrierCheck() -> void {
    // Only meaningful in S2. Snapshot the mode under the lock, then RELEASE it
    // before probing — ProbeWifiSta shells wpa_cli and can take seconds, and the
    // arbiter mutex serialises the BLE command thread, so we must not hold it
    // across the probe.
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (mode_ != ConnectivityMode::kStaOnly) {
            return;
        }
    }
    const bool carrier_up = uplink_.ProbeWifiSta().ok;

    std::lock_guard<std::mutex> lock(mtx_);
    // Re-check: a confirm/revert on another thread may have left kStaOnly while we
    // probed. If so this watchdog tick is stale — do nothing (and do not re-arm;
    // whatever transitioned us owns the timer now).
    if (mode_ != ConnectivityMode::kStaOnly) {
        return;
    }
    if (carrier_up) {
        carrier_loss_streak_ = 0;
        ArmCarrierWatchdogLocked();
        return;
    }
    if (++carrier_loss_streak_ >= kCarrierLossStrikes) {
        spdlog::warn(
            "ConnectivityArbiter: S2 carrier lost for {} consecutive checks — auto-reverting to "
            "P2P (D2a watchdog)",
            carrier_loss_streak_);
        DoRevertLocked();  // disarms the timer, restores the GO, resets the streak
        return;
    }
    spdlog::info("ConnectivityArbiter: S2 carrier check failed ({}/{}) — watching",
                 carrier_loss_streak_, kCarrierLossStrikes);
    ArmCarrierWatchdogLocked();
}

auto ConnectivityArbiter::OnEthernetCarrierPoll() -> void {
    // Probe the carrier WITHOUT the lock (off the BLE dispatcher; this runs on the
    // monitor thread).
    const bool carrier = ethernet_.ProbeEthernet().carrier;
    sst::config::UplinkData desired;
    std::string phone_subnet;
    std::string sta_ip;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        // Re-arbitrate on an actual up/down EDGE (R20), OR — F4 — while a DHCP-pending
        // candidate is held (carrier still up), so a lease that lands with NO carrier
        // transition still promotes Ethernet. The common no-change tick is a cheap
        // early return that never holds the lock across a shell-out.
        const bool edge = carrier != ethernet_carrier_;
        const bool pending_repoll = carrier && dhcp_pending_;
        if (!edge && !pending_repoll) {
            return;
        }
        spdlog::info(
            "ConnectivityArbiter: Ethernet {} (carrier={} was={} dhcp_pending={}) — re-arbitrating "
            "(mode-independent monitor, R20/F4)",
            edge ? "carrier edge" : "DHCP-pending re-poll", carrier, ethernet_carrier_,
            dhcp_pending_);
        ethernet_carrier_ = carrier;
        desired = desired_uplink_;
        phone_subnet = phone_subnet_cidr_;
        sta_ip = preview_endpoint_;
    }
    // F3: the heavy Ethernet work (ProbeEthernet + ProbeDhcpOffer + a different-subnet
    // uplink bind — each a potential 20s dhclient) runs OFF the lock, mirroring
    // OnCarrierCheck's lock-drop, so an Ethernet hotplug never freezes the BLE command
    // thread for ~40s. Re-take the lock, re-validate, then commit.
    const PreparedEthernet prepared = PrepareEthernetTransition(desired, phone_subnet, sta_ip);
    std::lock_guard<std::mutex> lock(mtx_);
    // Re-validate: a concurrent transition may have moved the carrier since the snapshot
    // — if so this poll is stale and the newer event owns the re-arbitration.
    if (prepared.eth.carrier != carrier) {
        ethernet_carrier_ = prepared.eth.carrier;
        return;
    }
    ArbitrateLocked(/*unsolicited=*/true, prepared);
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

auto ConnectivityArbiter::CurrentLanPath() const -> LanPath {
    std::lock_guard<std::mutex> lock(mtx_);
    return current_path_;
}

auto ConnectivityArbiter::LastError() const -> std::string {
    std::lock_guard<std::mutex> lock(mtx_);
    return last_error_;
}

auto ConnectivityArbiter::CurrentSsid() const -> std::string {
    std::lock_guard<std::mutex> lock(mtx_);
    return current_ssid_;
}

auto ConnectivityArbiter::EthernetEndpoint() const -> std::string {
    std::lock_guard<std::mutex> lock(mtx_);
    return ethernet_endpoint_;
}

auto ConnectivityArbiter::EthernetCredential() const -> std::string {
    std::lock_guard<std::mutex> lock(mtx_);
    return ethernet_credential_;
}

auto ConnectivityArbiter::EthernetConfirmId() const -> MigrationId {
    std::lock_guard<std::mutex> lock(mtx_);
    return current_path_ == LanPath::kEthernet ? current_id_ : 0;
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
