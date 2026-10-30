# Connectivity and Uplink v4 Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the backed-up single-radio connectivity and Ethernet/WiFi uplink work to protocol v4 without losing LAN SSH or the v4 session lifecycle.

**Architecture:** Add only non-conflicting connectivity messages to the current proto; port the backed-up arbiter, adapters and UI by subsystem on top of each repo's `development`. Reuse the existing v4 control plane and session ownership. Keep firmware and app on the same *merged* proto pin.

**Tech Stack:** proto3/buf, C++20/CMake/GTest/JetPack 7.2 networking, Flutter/Dart/Riverpod/Drift, git worktrees, act.

**Spec:** `/root/setup/connectivity-handoff.md`, `proto/bluetooth.proto`, and the old firmware `docs/plans/2026-07-13-001-feat-connectivity-architecture-plan.md` and `docs/plans/2026-07-15-001-feat-connectivity-single-lan-path-plan.md` on `origin/feat/connectivity-architecture`.

## Global Constraints

- Work only under `/root/wt/conn/<repo>`, on `feat/connectivity-uplink` from `origin/development`, with `HOME=/root`; do not reset or switch the product checkouts.
- Proto first: PR to `development`, merge after `lint`, `breaking`, `build`; firmware and app then pin its *merged* SHA. No backup branch is a PR head.
- Keep v4 health, device-time and snapshot handshake, session survival and internal dual-camera proxy. Do not replace v4 files wholesale.
- Firmware and app PRs stay ready but **unmerged**. No tag, sudoers change, direct protected-branch push, or hardware claim from container tests.
- Network transitions must fail safely: existing LAN SSH over Ethernet remains available even if nftables or STA association fails.

## Approaches

- Merge or replay entire old branches: fastest textual import, but clobbers v4 and reuses occupied protobuf tags. Rejected.
- Rewrite everything: clean integration, but throws away tested connectivity behavior. Rejected.
- **Selected:** transplant small commits/files by subsystem, adapt overlapping handlers and app screens to v4, and add targeted bug regressions before editing production code.

## Review Focus

- STA DHCP renewal while wired LAN is up must never install a competing metric-0 same-subnet route (Task 3).
- Firewall load failure on tegra must not lock out Ethernet SSH; install/reapply and fallback must be verified on-device before activation (Task 3).
- A stale migration/confirmation ID must not change LAN path or strand the app (Tasks 1, 2, 4).
- Reconnect during a recording/stream must preserve the v4 session, snapshot and proxy owner (Tasks 2, 4).
- App DB migration and dev seed must not erase existing match or network credentials (Task 4).

---

### Task 1: Additive proto contract

**Files:** `sst-cam-proto/bluetooth.proto`; existing `README.md` only if transport documentation needs correction.

**Interfaces:** Retain `Command.set_network_config=42`, `get_network_config=43`, `get_session_snapshot=45`, `set_match_state=46`, `set_device_time=47` and response `session_snapshot=30`. Add `LanPath` and preview/LAN confirmation messages from `origin/feat/connectivity-polish`; use unoccupied **Command 48/49** and **CommandResponse 31/32** for confirmations. Extend `SetNetworkConfigCommand` with `optional phone_subnet_cidr=2` and `NetworkConfigResponse` with fields 6–15; keep its 1–5 uplink fields unchanged. The `feat/network-config-uplink` and `feat/network-config-uplink-firmware` copies are identical and already in development.

- [ ] Add additive fields and messages to the current schema, preserving all v4 numbers and types.
- [ ] Run `buf lint`, `buf breaking` against `origin/development`, and `buf build`; confirm the non-breaking PR title `feat(proto): add connectivity and LAN-path handshake` agrees with the gate.
- [ ] Push, open PR to `development`, wait for `lint`, `breaking`, `build`, squash-merge and record merged SHA.

### Task 2: Single-radio arbiter and secure preview

**Files:** `src/{domain,app,adapters}/network/**`, `src/adapters/control/ble/bluez/**`, `src/app/control/services/handlers/{network,wifi-direct,streaming}.handler.*`, RTSP adapter, `src/main.cpp`, matching `tests/network/**` and `tests/control/**`.

**Interfaces:** Reuse the old `ConnectivityArbiter` for P2P↔STA preview/reachability and self-revert, preserve the v4 `GetSessionSnapshot`/`SetDeviceTime` dispatch and session cleanup. Maintain BLE bonding/encryption before credential writes and preserve v4 preview/record/stream health gating.

- [ ] Import focused old tests for WiFi handoff, confirmation, carrier-loss and streaming transport; adapt to v4 interfaces and observe expected failure.
- [ ] Port arbiter, timer, monitor, handlers and BLE/RTSP seams by subsystem; avoid replacing v4 session and proxy logic.
- [ ] Build and run focused tests before moving to uplink routing.

### Task 3: Ethernet uplink, safe routes and firewall

**Files:** `src/adapters/control/network/{ip-ethernet-command,ip-ethernet-configurator,net-iface,nmcli-uplink-configurator,policy-routing}.*`, `src/app/network/{ports,services}/**`, network handler, `src/main.cpp`, `deploy/{install.sh,sst-cam-firmware.service,install-test.sh}`, corresponding `tests/control/network/**` and `tests/network/**`.

**Interfaces:** The Ethernet LAN subnet route must outrank STA even after `dhclient -1 -4 wlP1p1s0` and renewal; choose an explicit STA route metric greater than Ethernet's 100 (e.g. 600) at route creation. Ethernet-as-uplink policy routing must retain LAN source replies on Ethernet. Firewall must not depend on the failing tegra `ct state established,related` expression; explicitly allow SSH from LAN on `enP8p1s0` and established traffic using supported primitives, make repeated loads safe, and preserve SSH if installation fails.

- [ ] Port the backup's `net-iface` and policy-routing tests; add failing regression for a same-subnet STA route that must not outrank wired LAN and for firewall install/reapply with LAN Ethernet SSH allowed.
- [ ] Port Ethernet static/DHCP, source policy routes, uplink probe, firewall and service installer; adapt to v4. Fix routes at the STA DHCP origin (not only via transient `ip route` changes), and avoid an overly permissive TCP-SYN-only fallback.
- [ ] Run focused C++ tests, installer tests and inspect the generated nft script before any metal test. Do not activate an unverified firewall remotely without a recovery path.
- [ ] If small, add an early `sst_cam_firmware --version` print-and-exit before hardware initialization, with a CLI smoke check.

### Task 4: App connectivity UI on v4

**Files:** `lib/core/{ble,wifi,models,db,state,shell}/**`, `lib/features/settings/{network,streaming}/**`, `lib/features/match/session/**`, `lib/mock/**`, Android WiFi channel/manifest, corresponding `test/**`, and `proto`.

**Interfaces:** Match the merged proto's new 48/49 and 31/32 confirmations; keep v4 reconnect health cycle, `session_actions.dart` split, and existing session/seed migrations. Regenerate Drift output against development's schema rather than copying the old generated file.

- [ ] Pin the merged proto SHA, regenerate Dart bindings, port network handshake and tests; observe focused test failure before implementation.
- [ ] Port Android WiFi handoff, Riverpod/network UI and mock parity while preserving v4 screen actions and old credential data.
- [ ] Run `flutter analyze` and `flutter test` in the app dev container; hot reload/restart if a running app is connected.

### Task 5: Integration and handoff

**Files:** `proto` pins in app/firmware, firmware PR body, app PR body.

- [ ] Pin exactly the same merged proto commit in both consumers; run proto pin checks and firmware `/root/setup/act/gen-act.sh`, `run-light.sh`, `run-heavy.sh` plus app container analyze/test before push.
- [ ] Review v4 regressions and diff, commit and push each consumer; open **ready, unmerged** PRs into development.
- [ ] Firmware PR body must ask Raul to approve/run Jetson checks: back up current binary and nft rules; maintain a separate SSH recovery path, test `nft -c -f`/atomic load and repeated load on kernel `6.8.12-1021-tegra`, verify Ethernet LAN SSH before/after firewall and STA join/renewal/P2P handoff, check `ip route get` sourced from `10.10.1.30`, inspect RTMP egress and reboot/reconnect; report actual outcomes before merging.
- [ ] Retain plan path, PR numbers, verification and remaining questions in Hindsight bank `sst-cam`.
