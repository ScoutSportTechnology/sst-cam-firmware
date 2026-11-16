# Vision Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Raul's 2026-11-16 decisions authorize incremental Tasks 1–2; retain later hardware and coordinated-release gates. Each numbered task is a separately mergeable change (or a separately mergeable proto/app/firmware sequence) and has its own review/CI gate.

**Goal:** Ship synchronized dual-4K GPU virtual-camera video at 1080p30, one clean shared H.264 encode, optional second overlay encode, and full-resolution still pairs instead of per-camera H.264 proxies.

**Architecture:** Keep 4K Argus buffers referenced in NVMM across a timestamp-paired 1–2 s ring. Feed VIC/TensorRT batch-two analysis to one panorama-space tracker/director, render only the chosen 1080p view from two full-res sources on GPU, and encode clean once before branching to MP4/RTSP/RTMP; overlay-on creates one separate encoded variant. Stage all changes behind an explicit deployment gate until measured on-device and the app/proto rollout is coordinated.

**Tech Stack:** C++20, GStreamer/Argus/NVMM, VIC, CUDA or VPI 4.1 (choose by measured interop gate), TensorRT 10.16, x264, GoogleTest/CTest, protobuf, Flutter app.

**Spec:** `docs/brainstorms/2026-11-16-vision-p2-design.md` (Raul's decisions incorporated; hardware gates remain).

## Global Constraints

- Raul's decisions are incorporated in the spec; implement Tasks 1–2 now, each on its own branch and PR. Keep the old pipeline default and the new path gated until calibrated and HIL-accepted. Hindsight reads currently fail with an MCP output-schema error.
- Both IMX477 use 3840x2160 NV12 @30 in NVMM; never copy a full-res frame to CPU at video cadence. Output is fixed 1920x1080 @30 for record/RTSP/RTMP; reject incompatible requests, drop 720p60 and independent quality choices. Analysis begins with synthetic detections; RF-DETR requires a separately reviewed artifact and accuracy gate. Delay targets 1.5 s / 45 pairs, configurable with measured memory gate with TensorRT loaded; fall back to 1.0 s / 30 pairs if needed, and stop if that fails. Pair nearest-PTS with configurable skew <= half frame + 2 ms (~18.7 ms @30), log every skew, counterpart wait max ~33 ms; tighten after phase-1b hardware sync evidence.
- Maximum one x264 for clean record+overlay-off RTSP/RTMP, maximum two when overlay-on and clean recording coexist; retain valid MP4 EOS/moov and RTSP auth/bind, RTMP routing, L1 and L2 semantics.
- START on the new pipeline requires both healthy cameras and validated real K/distortion/R, otherwise return "calibration not ready" to the app. During an active match, cam1 loss continues clean cam0 recording with degraded health; STOP remains usable. Do not assert 30 fps, zero-copy, or seamless blend until measured on the Jetson; stop for Raul if JP7.2 NVMM/VPI/CUDA interop fails or even a 1.0 s ring cannot fit.
- Firmware `just ci changed` in the pinned cross-compile container before each product PR; unit/module tests runnable there, real-camera tests committed with `hardware` CTest label. Tasks 1–2 require HIL timestamp/NVMM lifetime/ring-memory and still-pair evidence via `jetson-hil session`, at most four powered sessions total. Phase 1b independently adds `aeantibanding=3`; rebase on `development` before editing the capture adapter. JP7.2 ignores `camera_overrides.isp` (phase-1d extreme colour-matrix test); no ISP override ships. Task 4 stores per-camera 3×3 colour matrix + per-channel gain with calibration and matches cam0/cam1 in overlap, calibrated from a colour chart or overlap. For proto changes: proto PR/merge first, app+firmware re-pin the merged commit, check matching pins and run `just ci changed` in each touched repo; stop for Raul on a wire break. No new tools or dependencies unless compiler image/sysroot and CI are updated together.
- Domain and app ports never expose GStreamer/CUDA types; use adapter-owned referenced surfaces and neutral ownership/PTS metadata. Preserve match START/STOP, recording finalization, timeline export and existing historical recording downloads. Live overlay defaults OFF for both RTSP preview and RTMP. App-triggered BLE burst is 10 pairs at 2 Hz; periodic sampler is one pair/30 s. Proposed still quota is configurable 2 GiB pending real retention evidence. Legacy side-by-side/manual commands return explicit unsupported errors until coordinated removal. PAUSE leaves live clock running, L1 media PTS excludes pause, L2 skips paused overlay events.

## Review Focus

1. Invalid or incomparable camera timestamp epochs: refuse stereo pairing/still publication, mark synchronization unavailable; Task 1 tests this.
2. Dropped/restarted cam1 during an active recording: no stale frame pairing or duplicated training pair, preserve STOP/finalization and approved degradation policy; Tasks 1 and 3 test this.
3. RTSP subscriber attaches mid-GOP or RTMP reconnects: keyframe/SPS/PPS and monotonic PTS, no third encode or blocked clean recording; Task 5 tests this.
4. Record pause/resume with a dropped frame: L2 overlay follows L1 decoded PTS, not frame index or live clock; Task 6 tests this.
5. Disk full or partial two-image still write: no published half-pair, recording unaffected and old proxy files untouched; Task 2 tests this.

## File/dependency map

Paths below are design targets, not code written in this phase. Confirm current branch paths at execution because the workspace may have advanced.

| Unit | Ownership and files |
| --- | --- |
| NVMM capture/sync/ring | Modify `src/adapters/capture/frame/gstreamer/gstreamer.{hpp,cpp}`, `src/app/pipeline/services/orchestrator/pipeline-orchestrator.{hpp,cpp}` and `src/domain/capture/models/frame.hpp` or add an opaque video-surface handle beside it; create `src/domain/pipeline/services/frame-pair-synchronizer.{hpp,cpp}` and `frame-pair-ring.{hpp,cpp}`, add their formatters and tests under `tests/pipeline/`, `tests/capture/adapter/`; update `CMakeLists.txt` and `cmake/hardware-tests.cmake`. |
| Still sampler | Create `src/app/pipeline/ports/still-pair-sink.hpp`, `src/adapters/capture/stills/filesystem-still-pair-sink.{hpp,cpp}` and tests under `tests/storage/`; modify `src/main.cpp`, session/record/stream owner lifecycle and retention config/serde only if needed. Old proxy remains until final deletion. |
| Geometry/analysis | Create domain camera-ray projection, overlap dedupe, tracker/director models/services under `src/domain/vision/` with formatters, an app inference port under `src/app/vision/ports/`, and TensorRT+VIC adapter under `src/adapters/vision/`; tests under `tests/vision/`; extend per-camera `src/domain/config/models/calibration.hpp`, JSON serde/default validation, and build linkage. |
| GPU renderer | Create `src/app/pipeline/ports/virtual-camera-renderer.hpp`, `src/adapters/processing/gpu/virtual-camera-renderer.{hpp,cpp}` (VPI **or** CUDA after gate) and tests under `tests/processing/`; modify `src/app/pipeline/services/orchestrator/` and `src/main.cpp`. |
| Encode and delivery | Modify `src/adapters/storage/gstreamer/{recorder-launch,gst-continuous-recorder}.{hpp,cpp}`, `src/adapters/streaming/gst_rtmp/`, `src/adapters/streaming/gst_rtsp/`, `src/app/streaming/services/streaming_service/`, recording service and `src/main.cpp`; create encoded-AU distribution adapter/port under `src/adapters/storage/gstreamer/` / `src/app/streaming/ports/`; test `tests/storage/`, `tests/streaming/`, `tests/pipeline/`. |
| Overlay/timeline | Keep `src/adapters/overlay/cairo/`, `timeline/` and `burn/`; update timestamp mapping and `src/adapters/overlay/burn/opencv-overlay-burner.cpp`; reuse scene cache, add conditional GPU overlay upload/composite in renderer; test `tests/overlay/`, `tests/export/`. |
| Contract/cleanup | Proto repo `bluetooth.proto`, firmware handlers/session snapshot and tests; app `lib/core/ble/`, `lib/core/widgets/preview_layout_toggle.dart`, session/setup, `lib/mock/emulator/mock_ble_service.dart` and matching tests. At final switch remove old `src/adapters/raw_capture/` proxy writers/retention, obsolete decision and side-by-side modules only when no references remain; preserve legacy file exclusion in download server. |

## Merge sequence (TDD within each task)

### Task 1: NVMM capture, common-clock pairing and delayed ring

**Interfaces:** `VideoSurface` owns one referenced NVMM buffer plus producer fence, camera index, monotonic capture PTS, geometry; `FramePairSynchronizer::Push(camera_index, VideoSurface) -> optional<FramePair>` returns nearest-PTS pairs only within configured half-frame + 2 ms (~18.7 ms at 30 fps), logging each pair's skew; `FramePairRing::Push(FramePair)` and `PopDue(now, configured_delay) -> optional<FramePair>` retain at most the configured 45 pairs / byte budget, falling back to 30 pairs / 1.0 s only after memory measurement with a TensorRT engine loaded. No GStreamer type in domain/app headers.

**Files:** Capture/sync/ring entries in file map; modify orchestrator only to construct a parallel **gated** capture path (legacy default untouched); add labeled real-camera sync test.

- [ ] Write isolated failing tests: pair closest by normalized capture PTS at 18 ms; 19 ms rejected at 30 fps; out-of-order/duplicate PTS and incomparable epochs rejected; unmatched frame discarded after ~33 ms; cap 45 pairs and bytes, verify 30-pair fallback and release a surface only after the last GPU reader fence; camera restart flushes pending pairs and keeps STOP available.
- [ ] Run `just test 'FramePair|FramePairRing|GStreamer'`; confirm new assertions fail for the intended reason.
- [ ] Implement GStreamer 4K NVMM appsink ownership without `gst_buffer_map`, timestamp normalization validation, bounded queues, ring and camera-health recovery; retain existing default path. Add synthetic-buffer port tests and hardware test for two real IMX477 timestamp origins, 30 min drift, p99 skew and no 4K CPU maps.
- [ ] Run focused tests and `just ci changed`; HIL via one `jetson-hil session` measures common-clock PTS skew distribution, NVMM lifetime/interop, 45-pair peak/steady memory with TensorRT engine loaded, then 30-pair fallback if needed; stop on failed interop or 30-pair memory. Commit only this task's files, open PR, merge only with green CI. Do not enable the new pipeline by default.

### Task 2: Atomic paired still sampler (no proxy deletion yet)

**Interfaces:** `IStillPairSink::Offer(const FramePair&, match_uuid) -> bool` is a nonblocking bounded enqueue; `RequestBurst(match_uuid, count=10, rate_hz=2)` is app-triggered through a new BLE command in coordinated Task 7; Task 2 provides the internal request interface without a proto change. Published pair directory contains cam0/cam1 stills + manifest with capture timestamps, skew and calibration version; retention skips active/temporary groups and never deletes historical proxies.

**Files:** Still sampler entries in map and existing `src/app/raw_capture/ports/proxy-owners.hpp`, `src/app/control/services/handlers/{recording,streaming}.handler.cpp`, `src/app/session/services/session_cleanup/`, config/storage paths as needed. Keep existing proxy writing during this task so each merge works independently.

- [ ] Write failing module tests: one pair/30 s in active match, 10-pair burst at 2 Hz, no output for skew > configured half-frame + 2 ms, nonblocking queue overflow drops sample, failed image/disk write never publishes half-pair, group collision never overwrites, stop/disconnect drains boundedly, legacy proxy files stay untouched.
- [ ] Run `just test 'StillPair|ProxyOwners|RecordingHandler|StreamingHandler'` and observe the new tests fail.
- [ ] Implement selected-frame-only transfer, image encoding and atomic **directory** publication with bounded queue; preserve match UUID lifecycle and old downloads. Add `hardware` tests for 4K pair image integrity, burst CPU spikes and simultaneous MP4 continuity.
- [ ] Run focused tests and `just ci changed`; HIL via `jetson-hil session` verifies real paired 4K still integrity, periodic/burst timestamps, memory/CPU spikes and simultaneous MP4 continuity; commit, open PR and merge only with green CI. Do not turn off proxies or add the BLE command before the coordinated Task 7 release.

### Task 3: Calibrated panorama-space analysis and director

**Interfaces:** `Project(camera_index, pixel_box, K, distortion, R) -> angular_box`, `Dedupe(vector<angular_box>, pair_pts) -> vector<Detection>`, `Tracker::Update(detections, pts) -> Tracks`, `Director::Pose(tracks, output_pts) -> {yaw,pitch,zoom}`. `IDetector::Detect(two VIC-downscaled surfaces) -> DetectionBatch` is an app port, with deterministic synthetic adapter for integration and a TensorRT RF-DETR adapter only once weights/classes/shape and accuracy gate are approved.

**Files:** Geometry/analysis entries in map and `tests/vision/`; do not replace old `IDecision` yet.

- [ ] Write failing tests for K/distortion/R ray alignment, seam duplicate becoming one track ID, no overlap preserving both detections, stale/empty batch yielding safe pose, camera restart resetting affected tracks, and one 10–15 Hz batch-of-two scheduling test with a synthetic detector boundary.
- [ ] Run `just test 'Vision|Director|Tracker'`; observe intended failures.
- [ ] Implement pure math/tracker/director; validate calibration JSON (per-camera 4K K/distortion plus relative R, version and geometry) and forbid placeholder defaults as real calibration. Implement synthetic detections first; implement VIC downscale + TensorRT batched inference only after separately reviewing the RF-DETR artifact and passing its accuracy gate; no CPU readback beyond boxes. Synthetic detector stays explicit and nonproduction.
- [ ] Run focused tests and `just ci changed`; commit. Add `hardware` tests of actual batch engine latency/accuracy and `tegrastats` load before directing default footage; if unavailable, keep static central pose under the gate and record the limitation.

### Task 4: Direct GPU virtual-camera render and delay

**Interfaces:** `IVirtualCameraRenderer::Render(const FramePair&, Pose, calibration) -> RenderedSurface` returns one 1920x1080 NVMM NV12 surface + PTS/fence; the orchestrator consumes a ring-due pair and last eligible director pose (not a raw camera index/crop). Reuse old snapshot boundary only on demand.

**Files:** Renderer entries in map, orchestrator, `src/main.cpp`, tests and hardware-label list.

- [ ] Write failing golden-ray/fixture tests for both single-camera views and seam crossing, invalid/missing real K/distortion/R refusing new-pipeline match START with "calibration not ready", black/valid border behavior, per-camera 3×3 colour matrix and per-channel gain producing matched cam0/cam1 overlap from colour-chart or overlap calibration, 30 Hz cadence on one missing pair with at most one duplicate, and bounded GPU surface lifetime.
- [ ] Run `just test 'VirtualCamera|PipelineOrchestrator'`; observe failures.
- [ ] Implement *one* VPI or CUDA remap adapter after the interop trial: sample directly from the two 4K referenced sources, apply calibrated per-camera 3×3 colour matrix and per-channel gain and match colour before GPU feathering the seam when both contribute, output NV12 into bounded GPU surface pool, fence before release. Leave old pipeline default and expose explicit deployment gate; do not build a stitched full-res frame.
- [ ] Run focused tests and `just ci changed`; commit. Add labeled hardware tests for overlap (requires real overlapping rig), non-overlap cam0 fallback, GPU/VIC/EMC timing, 1–2 s delay and 30-minute bounded memory; **do not claim seam validated on the current non-overlap rig**.

### Task 5: One encoded clean AU bus for MP4, RTSP and RTMP

**Interfaces:** `IEncodedVideoSubscriber::OnAccessUnit(Au{data, pts, dts, duration, keyframe, codec_config})`; `EncodedVideoBus::Subscribe(...)` owns bounded independent clean subscribers. Encoder starts while any clean recorder/preview/stream needs it; recorder receives all clean AUs; RTSP is lazy per connected viewer but no viewer-specific x264.

**Files:** Encode/delivery entries in map and tests. Record and live quality are fixed 1920x1080@30; reject other requests explicitly when new pipeline is active.

- [ ] Write failing module tests: 30 clean output frames/s → one x264 instance shared by MP4+RTSP+RTMP; branch with stalled network drops/resyncs at IDR without blocking recorder; new RTSP viewer gets codec config and keyframe; reconnection preserves PTS order; STOP waits EOS and validates playable MP4, pause/resume continues same MP4 and forces IDR. Test with real GStreamer loopback/testsrc in container, not mocks of the encoder/mux.
- [ ] Run `just test 'Recorder|Rtmp|Rtsp|Streaming|EncodedVideo'`; verify failures.
- [ ] Implement VIC NVMM→system I420 **once**, single x264, parsed AU tee before MP4 mux, bounded independent subscriber queues, parser/payloader/mux per transport. Keep RTSP bind/auth, RTMP audio/reconnect and recording disk collision/finalization semantics; old stream path stays available behind gate until real downstream works. Never let network or an appsrc with `block=true` stall recorder.
- [ ] Run focused tests and `just ci changed`; commit. On HIL compare CPU/frames/latency of record + RTSP + RTMP and finalized 30+ min MP4; do not switch default if >1 encode exists in overlay-off mode or corrupt file appears.

### Task 6: Optional second overlaid encode and PTS-correct L2

**Interfaces:** `LiveOverlayPreference::Set(bool)` (default OFF, wire mapping in Task 7); `EncodedVideoBus` creates overlaid encoder only while overlay-on stream/preview subscribed. `OverlayTimeline` maps event clock to **recorded media PTS**, including paused spans; `OpenCvOverlayBurner` reads decoded media PTS rather than frame index.

**Files:** Overlay/timeline entries in map, renderer and output bus, recording service, `tests/overlay/` and `tests/export/`.

- [ ] Write failing tests: clean L1 pixel content regardless of live toggle; overlay-off one encode, overlay-on plus clean recording exactly two, overlaid RTSP+RTMP share one; toggle mid-session moves viewer at IDR; PAUSE and dropped encoded frames replay L2 events against decoded media PTS with live clock continuing, L1 media PTS excluding pause and L2 skipping events in paused spans; export blocked during active live session.
- [ ] Run `just test 'Overlay|Export|EncodedVideo'` and observe failures.
- [ ] Implement cached-scene GPU upload/composite for live 1080p only, conditional second x264, existing Cairo scene/timeline persistence, and PTS-aware offline export; preserve `OpenCvOverlayBurner` for the offline job rather than replacing its codec path.
- [ ] Run focused tests and `just ci changed`; commit. HIL measures simultaneous 2× x264 + inference + still burst and validates 30+ min MP4/RTSP/RTMP; if budget fails, report measured trade-offs before enabling overlay ON.

### Task 7: Coordinated proto/app contract and default switchover

**Interfaces:** Retain wire numbers and reserved proxy fields. Add optional overlay-preference (default OFF for RTSP/RTMP) and app-triggered burst command/status, preserving wire numbers; stop for Raul if the proposed proto change is a wire break. Existing `SetPreviewLayout`/`SetActiveCamera` must never return success for an unsupported view; the app hides/removes their controls and snapshot reconciliation handles old devices. Record/stream mode negotiation must not promise independent modes if both share AUs.

**Files:** Proto `bluetooth.proto`, proto tests; firmware device/record/stream/preview/session handlers, `src/main.cpp`, quality mode table and tests; app `lib/core/ble/ble_protocol.dart`, `ble_service_impl.dart`, session/setup widgets and state, `lib/core/widgets/preview_layout_toggle.dart`, mock BLE and widget/contract tests; app `docs/firmware-spec.md`. Confirm exact app filenames before edits.

- [ ] Write failing proto compatibility/breaking tests and app+firmware contract tests: explicit overlay OFF/ON across reconnect and mock, burst response, no obsolete `Both`/manual selection in new UI, legacy device's command response, fixed record/live capabilities, STOP/finalization unaffected.
- [ ] Implement proto **first**, run proto `just ci changed` and merge only after green checks and approval of implementation. For removed/renamed fields, mark `reserved` and bump `DeviceInfoResponse.protocol_version`; every wire break has `feat(ble)!:` PR title. Then pin the **merged** proto commit in both consumers (same branch name), regenerate bindings, implement handlers/app/mock and run workspace pin check plus `just ci changed` in each consumer.
- [ ] Only after both consumer builds and HIL acceptance switch default runtime to GPU path. Check app real/mock reconnect, recording clean download, RTSP preview and L2 overlay export. Coordinate firmware/app release so unsupported old combinations are explicit, not silently misencoded.

### Task 8: Retire unused CPU/proxy/decision paths safely

**Interfaces:** New tracker/director and still sampler replace the old production `IDecision` camera selector and proxy owner/sink; keep historical file exclusions and download security. Do not delete user footage from device.

**Files:** Delete `src/adapters/raw_capture/{filesystem-raw-capture-sink,proxy-launch,proxy-retention}.{hpp,cpp}`, old raw capture port/owners/naming and their tests after migrating consumers; delete old decision/manual/static code and side-by-side compositor and tests after contract migration; remove hot-path old preprocess/postprocess/materialize and unused `src/adapters/overlay/gstreamer/gst-overlay-compositor.*` **only if** all callers/tests are migrated. Update `CMakeLists.txt`, `src/main.cpp`, config and repo `AGENTS.md` stale pipeline descriptions as applicable.

- [ ] Write failing migration tests asserting historical `raw__*.mp4` cannot be minted/downloaded via recordings catalog and are not deleted by new still retention; all match/stream/cleanup paths use still ownership and never create proxy encoders.
- [ ] Run focused storage/download/session tests and verify failures; remove obsolete implementation and references in one reviewable change without removing Cairo timeline or offline burn.
- [ ] Run focused tests, `just ci changed`, cross-repo pin check and later HIL full-session acceptance; commit. Search for obsolete public references before declaring cleanup done. Keep the old pipeline as default until gated HIL acceptance; do not retire proxies during Tasks 1–2.

## Plan self-review / execution gates

Coverage: capture/memory/sync (1), sampler (2), AI/calibration/tracker/director (3), GPU remap/delay (4), encoded clean bus (5), overlay/L2 (6), app/proto (7), deletion (8). Each task starts with red tests and ends with container CI; hardware validation is separately gated. Review Focus is pinned to Tasks 1/2/3/5/6. Raul's decisions fix the pairing, memory, calibration, output, sampler, overlay, compatibility and pause constraints. Tasks 1–2 are authorized independently; remaining work is gated on hardware, calibration, and coordinated proto/app rollout.
