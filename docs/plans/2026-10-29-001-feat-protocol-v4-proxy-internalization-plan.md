# Protocol v4 and Internal Proxy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) for tracking.

**Goal:** Make firmware implement the v4 reconnect/health lifecycle expected by app PR #43, and keep proxy capture entirely internal while exposing persistent L2 exports as normal recordings.

**Architecture:** Extend existing BLE handlers and session manager, not a second control plane. Keep capture facts and disconnect deadline in the running service; use existing recording/stream cleanup and filesystem catalog for finalization and downloads. Start-class operations consult frame-truth health, not merely a running pipeline. No unsolicited BLE writes.

**Tech Stack:** C++20, protobuf from proto PR #18 (`16cafde` during development), GTest, CMake/Conan, GStreamer, existing `act` firmware workflows.

**Spec:** `proto/docs/brainstorms/2026-07-06-state-health-quality-cycle-requirements.md`, `proto/docs/plans/2026-07-06-001-feat-state-health-contract-plan.md`, app PR #43's `docs/plans/2026-07-06-001-feat-state-health-quality-cycle-app-plan.md`, and `proto/bluetooth.proto`.

## Global Constraints

- Branch `feat/retire-proxy-wire` from `origin/development`; do not edit proto or app. Pin proto branch commit only until proto #18 merges; CI proto pin check is expected to fail until repinned to the merged commit.
- Protocol version **4**; new scalar fields are optional and read with `has_*()`. Absent `auto_stop_minutes` means 30; absent absolute-set fields mean unchanged, including explicit zero/false.
- Reconnect sequence is device-info protocol gate → phone time → snapshot → absolute match reconciliation. BLE disconnect never destroys an active match; timeout is monotonic and independent of match-clock pause.
- No DB, new dependency, push event, proxy wire command/metadata, proxy download, exports directory, or post-download L2 deletion. Preserve stream/record shared proxy ownership, existing finalization/error handling, and stop/download availability while unhealthy.
- Commit each completed unit; preserve hardware-only tests and report what could not be checked on metal.

## Review Focus

- Disconnect while recording/streaming: both continue and reconnect snapshot identifies same match UUID, clock and activities (Task 2).
- Repeated start/stop with both stream and record owning proxy: no early proxy stop or leaked capture (Task 5).
- Explicit zero and false in absolute state set: overwrite, while absent fields preserve previous values (Task 3).
- Camera with no frames, recovering watchdog, or permanently down: health reflects truth; start is rejected with `DEVICE_INOPERABLE`, stop/download allowed (Task 4).
- Timeout during finalize, rejoin just before timeout, or reboot orphan: no double finalize; idle summary accurately reports end reason and file validity (Task 2).

---

### Task 1: v4 compile and wire compatibility

**Files:** `proto` submodule; `src/app/control/services/handlers/device.handler.{hpp,cpp}`; `tests/control/device_handler.test.cpp`; retired proto usages throughout handlers/tests; build registrations if retired handler is removed.

**Interfaces:** `DeviceInfoResponse.protocol_version = 4`; existing telemetry remains pull-only. Generated v4 proto is sole source of field/message names.

- [x] Add/adjust a failing device-info version test and remove tests that assert tombstoned fields.
- [x] Build test preset to identify every retired-field compilation failure; delete retired wire references, raw-command registration/handler and its test, without removing the internal sink.
- [x] Set protocol version to 4 and pass the device-info test; commit `feat(ble)!: adopt v4 contract and retire raw wire handler`.

### Task 2: session survival, truthful snapshot, and bounded auto-stop

**Files:** `src/{domain,app}/session/**`, `src/app/control/services/handlers/session.handler.{hpp,cpp}`, `src/app/control/services/handlers/recording.handler.{hpp,cpp}`, `src/app/session/services/session_cleanup/**`, `src/main.cpp`; `tests/session/session_manager.test.cpp`, `tests/control/control_plane_integration.test.cpp`, `tests/control/recording_handler.test.cpp`.

**Interfaces:** `GetSessionSnapshotCommand` returns `SessionSnapshotResponse`; `PushSessionConfigCommand.auto_stop_minutes` uses `has_auto_stop_minutes()`; session retains its identity/actual activity through BLE loss, and produces idle-only `LastSessionSummary`. Explicit app stop, timeout, irreversible camera failure and reboot orphan map to proto end-reason enum. Auto-stop deadline is armed on disconnect and cancelled on rejoin; config update re-arms using latest duration.

- [x] Write failing module-boundary tests for record/stream disconnect survival, reconnect-before-deadline cancellation, default/custom timeout, single finalization, idle summary and orphan file validity.
- [x] Implement monotonic deadline in existing runtime tick, reuse recording/stream/WiFi teardown and timeline finalization, expose `FINALIZING` during actual cleanup; never end capture merely because BLE disconnected.
- [x] Write failing snapshot tests for optional presence, `CONFIGURED`/`READY`/`RECORDING`/`FINALIZING`/`IDLE`, elapsed from record start, camera/layout/flags, last summary only idle; map response in session handler. Camera health is mapped from an injectable provider but production frame-truth health is deferred to Task 4.
- [x] Run focused tests under devcontainer/CI container and commit `feat(session): survive disconnect and expose bounded session snapshot`.

### Task 3: absolute match state and safe wall time

**Files:** `src/app/control/services/handlers/{match.handler,match-state.handler,device.handler}.{hpp,cpp}`, `src/domain/session/models/**`, `src/main.cpp`; `tests/control/{match_handler,match_state_handler,device_handler}.test.cpp`.

**Interfaces:** `SetMatchStateCommand` changes only present fields via locked match update; `MatchState.elapsed_seconds` stays monotonic, `time_remaining_s` clamps to zero, `clock_running` and `match_uuid` are optional when available. `SetDeviceTimeCommand.epoch_ms` rejects absent/pre-2020 values; injectable clock-setting seam reports OS failure; sync only wall clock, never monotonic match/session clocks.

- [x] Add failing tests for partial updates, explicit zero/false, paused/unpaused elapsed across ticks and overrun, UUID presence, pre-2020 and OS time-setting failure.
- [x] Implement with existing state/clock handler and `has_*()`; map a real system clock setter from `main.cpp`, not an in-memory fake for production.
- [x] Run focused tests and commit `feat(ble): reconcile match state and synchronize wall time`.

Task 3 verification (2026-10-29): devcontainer aarch64 test-preset cross-build succeeded; focused 27/27 passed; CI-excluded `ctest -j 8` 419/419 passed in 124.40 s. Changed C++ files passed clang-format-18 dry-run and clang-tidy-18 (nine changed translation units, zero diagnostics). System clock permission and live Jetson/BLE behavior remain untested in the container; proto pin check awaits proto PR #18 merge.

### Task 4: frame-truth camera health and start gating

**Files:** `src/app/pipeline/services/orchestrator/**`, camera watchdog/capture adapters, `src/app/control/services/handlers/{device,recording,streaming,wifi-direct,preview}.handler.*` as applicable, `src/main.cpp`; `tests/control/{device_handler,recording_handler,control_plane_integration}.test.cpp` plus camera/pipeline tests.

**Interfaces:** Both `DeviceTelemetry.camera{0,1}_health` and snapshot health are `CameraHealth`; OK only on observed frame, RECOVERING during restart, DOWN on failed restart. Return `DEVICE_INOPERABLE` on preview/record/stream starts if either required camera is not OK; stop/finalize, download, diagnostics, WiFi-Direct, reboot remain available.

- [x] Add failing tests for unknown/no-frame, OK, recovering/down transitions, start rejection and continued stop/download.
- [x] Reuse pipeline frame observations/watchdog state; share the health read between telemetry/snapshot and start guards rather than guessing from pipeline running state.
- [x] Run focused tests and commit `feat(health): report frame-truth health and gate starts`.

Task 4 ruling: The pinned proto requires `DEVICE_INOPERABLE` for every start-class command when either camera is not OK; this includes UNKNOWN and RECOVERING, despite the app mock allowing RECOVERING. `StartWifiDirect` continues to form the group when cameras are unhealthy so downloads remain available, but defers the RTSP preview until both cameras are OK; reissuing the idempotent command after recovery starts preview without reforming the group. Record and platform-stream starts are gated. An already-connected preview viewer during a later camera failure is not forcibly disconnected. The watchdog waits up to 2 seconds without a fresh frame, attempts one serialized restart, then reports DOWN if the restart fails; a delivered frame restores OK. A permanently failed camera ends an active session with CAMERA_FAILURE, retaining existing finalization semantics. Real Jetson/Argus timing and the app's soft RECOVERING UI remain integration caveats.

Task 4 verification (2026-10-29): devcontainer aarch64 test-preset cross-build succeeded; CI-excluded `ctest -j 8` 431/431 passed. Changed C++ files passed clang-format-18 dry-run and clang-tidy-18 (13 changed translation units, zero diagnostics). Real Jetson/Argus and BLE/RTSP behavior remain untested on metal; proto pin check awaits proto PR #18 merge.

### Task 5: internal ref-counted proxy and hidden files

**Files:** `src/app/control/services/handlers/recording.handler.*`, streaming lifecycle handlers, `src/app/session/services/session_cleanup/**`, `src/adapters/raw_capture/**`, `src/app/network/services/download_server/**`, `src/app/control/services/handlers/download.handler.*`, `src/main.cpp`; corresponding recording, streaming, raw capture, download and retention tests.

**Interfaces:** Proxy group identifier derives from validated `match_uuid`; proxy files go inside match directory and lifecycle starts automatically with record/stream while preserving shared owners. Proxy filenames remain internal; `Enumerate`, `ResolveRecordingPath` and token minting never reveal/serve them, even by guessed ID. Recording metadata no longer includes retired raw fields.

- [x] Add failing tests for record-only, stream-only, overlapping owners, disconnect/rejoin, filename/directory and guessed proxy download denial.
- [x] Remove all uses of retired `capture_group_id` wire field; derive safe internal ID and keep existing sink's ownership/ref-count implementation if present, otherwise implement minimal owner accounting at the lifecycle seam.
- [x] Filter proxy before catalog listing and path/token resolution; preserve retention without requiring download tokens for proxies; run focused tests and commit `feat(storage): internalize proxy lifecycle and hide proxy downloads`.

Task 5 verification (2026-10-29): devcontainer aarch64 test and debug cross-builds succeeded; focused record, stream, owner, catalog, download and retention tests passed, including start/finalization races. CI-excluded `ctest -j 8` passed 445/445 with the workflow's exact exclusion expression (the unexcluded suite failed six hardware-bound tests as expected). Changed C++ files passed clang-format-18 dry-run and clang-tidy-18 with zero diagnostics. Proxy start remains best-effort like the existing recording path: invalid match identity, absent/mismatched match folder, collision or sink failure leaves the primary record/stream running without proxy; no hardware encode, BLE or HTTP listener was checked on metal. Proto pin check still awaits proto PR #18 merge.

### Task 6: persistent L2 export and complete verification

**Files:** `src/main.cpp`, `src/app/export/services/export-job-manager.*`, `src/app/network/services/download_server/**`; `tests/export/export_job_manager.test.cpp`, `tests/network/download_server.test.cpp` and export handler tests.

**Interfaces:** Burned L2 is in the same match directory as L1, with distinct stable stem, listed and downloadable via ordinary recording and token flow; no automatic deletion on download or token expiry.

- [ ] Add failing export/catalog/token tests for same-folder L1+L2 and persistent L2 after token consumption/expiration.
- [ ] Change production export output path, remove any post-download L2 deletion, run focused tests and commit `feat(export): retain L2 beside L1 as a downloadable recording`.
- [ ] Run `devcontainer exec --workspace-folder . bash -lc 'cmake --preset test && cmake --build --preset test && ctest --preset test'` as feasible; run `/root/setup/act/run-light.sh` and `/root/setup/act/run-heavy.sh` from the firmware repo, inspect `/var/log/act-*.log`, and fix all `format`, `tidy`, `test`, `ci-scripts` failures except the known unmerged-proto pin check. Review `git diff`, push branch, open draft PR to `development` with the specified breaking title and contract questions. Do not merge.

## Task 2 rulings and PR questions

- Ruling: Same-match config re-push while recording updates only the disconnect timeout and retains capture paths, overlay and live match state; a different-match push remains rejected. The old ordering gate rejected every mid-recording push, conflicting with the v4 auto-stop reconfiguration contract.
- Ruling: Stop of the last recording or platform stream ends the session and tears down WiFi; stopping one while the other is active leaves the session up. Explicit StopWifiDirect is refused while either activity is active, preventing loss of an active match.
- Ruling: A persisted `.session-active` marker beside a recording identifies a boot orphan; the boot scan reports REBOOT with `file_valid=false` and removes the marker. It does not validate an orphaned MP4's moov; the validity is deliberately conservative. A failed EOS leaves its marker for the next boot scan.
- Ruling: Snapshot camera health remains absent until the Task 4 frame-truth provider is wired in production; an injected provider is mapped and tested in Task 2. `FINALIZING` is visible during synchronous cleanup, and `is_recording` is false once finalization has been claimed; clarify whether the UI expects the flag to remain true until EOS completes.
- PR question: A normal SIGTERM/fatal shutdown uses REBOOT as the end reason (no on-wire shutdown reason); confirm whether a new end reason is needed for clean service shutdowns.

## Contract questions to carry to PR

- Proto text says reject start when health is not `OK`; app mock rejects only `DOWN` and app UI treats `RECOVERING` as soft. Prefer strict proto safety for firmware, annotate difference.
- Proto describes idle WiFi-group timeout while mock only arms for recording/streaming; implement proto's idle-group safety if possible.
- Proto has stale proxy/L2 comments (raw-capture disconnect and post-download deletion) despite reserved wire fields and persistent-L2 intent. Do not edit proto here.
