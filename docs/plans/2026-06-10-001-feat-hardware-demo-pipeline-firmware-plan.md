---
title: "feat: Hardware-demo pipeline — dual capture, decision seam, software encode, raw capture"
type: feat
status: completed
date: 2026-06-10
origin: docs/brainstorms/2026-06-10-hardware-demo-firmware-requirements.md
---

# feat: Hardware-demo pipeline — dual capture, decision seam, software encode, raw capture

## Summary

Make the firmware pipeline run end-to-end on the real Jetson Orin Nano for the intelligence-deferred demo: replace the hardware-encode assumption with software H.264 (the Orin Nano has no NVENC), extract the inline cam-0 crop into a real `IDecision` seam, run **both** cameras through the orchestrator, actually start the already-built RTSP preview server in production, add an independent raw dual-camera capture command, and confirm storage + overlay + RTMP work against the new pipeline. Most modules already exist and are wired in `src/main.cpp` — this is largely fix-and-wire plus the decision/dual-camera/raw-capture additions, not build-from-zero.

---

## Problem Frame

The demo must prove the full non-AI pipeline on real hardware before the 3-week full-operation deadline (see origin: `docs/brainstorms/2026-06-10-hardware-demo-firmware-requirements.md`). Phase-1 research overturned two assumptions baked into the origin doc (which trusted the repo's stale status tables):

1. **The pipeline already exists.** `PipelineOrchestrator` (`src/app/pipeline/services/orchestrator/`) already wires capture → preprocess → materialize → buffer → postprocess → fan-out to storage + streaming, with a **fixed full-frame crop** that is exactly the "no-AI, cam-0, full-frame" decision the demo wants — but inlined in `ConsumerLoop`, single-camera, and the RTSP preview server is never started in production.
2. **The Orin Nano has no hardware encoder.** All three H.264 encode sites (`gst-continuous-recorder.cpp:54`, `gst-rtsp-app-stream-server.cpp:31`, `gst-rtmp-streamer.cpp:46`) hardcode `nvv4l2h264enc`, which does not exist on this silicon. As written, recording, preview, and RTMP all fail on the actual demo hardware. This is the highest-severity gap and the integration risk the demo exists to surface.

---

## Requirements

- R1. Both cameras run capture → preprocess → materialize → per-camera buffer on real worker threads; orchestration is start/stoppable with basic running-state reporting usable by telemetry. (origin R1, R3)
- R2. Static decision selects cam 0, full frame, behind a real swappable port (the intelligence seam). (origin R4, R5)
- R3. H.264 encode works on the Orin Nano (software encode) across recording, preview, and RTMP. (origin R6, R7, R12, R13 — prerequisite)
- R4. RTSP preview is served in production over WiFi Direct. (origin R6/R12)
- R5. Final (cam-0) recording writes to disk and is downloadable. (origin R6, R7)
- R6. Raw dual-camera capture records both cameras to per-camera files, independent of the final path. (origin R8, R9)
- R7. Overlay renders app-authored banner/scoreboard and composites onto final frames when enabled. (origin R10, R11)
- R8. RTMP/HLS platform streaming pushes the (optionally overlaid) final frames. (origin R13)
- R9. Module-boundary tests for new code; hardware-bound tests written and committed (expected to fail in container, pass on-device). (origin R14, R15)

**Origin actors:** A1 Operator, A2 App, A3 Firmware, A5 Streaming platform
**Origin flows:** F2 (live preview), F3 (record+transfer), F4 (raw dual capture), F5 (overlay+broadcast)
**Origin acceptance examples:** AE1 (orchestration), AE2 (record+download), AE3 (raw), AE4 (overlay), AE5 (preview/RTMP)

---

## Scope Boundaries

- No AI/tracking, physics, or dynamic decision — `StaticDecision` (cam 0, full frame) only.
- No microphone/real audio — RTMP gets a silent AAC track (platforms require an audio track).
- No dynamic camera switching/zoom.
- No new overlay authoring — render existing app-authored state only.
- No DB introduction — recording metadata stays filesystem-enumerated (there is no SQLite module today, despite stale docs).

### Deferred to Follow-Up Work

- Single-encode `tee` consolidation (one x264enc feeding RTSP + RTMP + recording) — only if on-device CPU measurement shows three independent software encodes don't sustain. Contingency, see U9 + Risks.
- HLS variant (the demo targets RTMP to the platform); `hlssink2` is available in already-installed plugins-bad if needed later.
- Emulator / remote on-device test execution (separate session).

---

## Context & Research

### Relevant Code and Patterns

- **Orchestration:** `src/app/pipeline/services/orchestrator/pipeline-orchestrator.{hpp,cpp}` — two threads (`ProducerLoop`/`ConsumerLoop`), `LatestOnlySlot<FrameBundle>`, inline full-frame `CropRect` at `pipeline-orchestrator.cpp:92-116`. Ctor takes `unique_ptr<ICaptureFrame>, IPreprocessor, IPostprocessor, IFrameSink&, PipelineConfig`. Single-camera today (`main.cpp:206-211`, `kCamera0Index = 0`).
- **Capture:** `src/adapters/capture/frame/gstreamer/gstreamer.{hpp,cpp}` — `GStreamerAdapter(CameraConfig, device_model, camera_index)`; `sensor-id=N`; 5-buffer appsink cap. Construct a second instance with index 1 for dual.
- **Buffer:** `LatestOnlySlot<T>`, `DropOldestRing<T>`, `MaterializeFrame` (`src/domain/buffer/`). `FanOutSink` (`src/app/buffer/services/fan_out_sink/`) forwards a final frame to multiple `IFrameSink`s.
- **Encode sites (all use the missing `nvv4l2h264enc`):** `src/adapters/storage/gstreamer/gst-continuous-recorder.cpp:54`; `src/adapters/streaming/gst_rtsp/gst-rtsp-app-stream-server.cpp:31`; `src/adapters/streaming/gst_rtmp/gst-rtmp-streamer.cpp:46`.
- **Streaming:** `StreamingService` (`src/app/streaming/services/streaming_service/`) is a fan-out sink wrapping the RTSP app-stream server + RTMP streamer. `StartAppStream()` exists but has **no production caller** (only tests). `GstRtspAppStreamServer` (`src/adapters/streaming/gst_rtsp/`) backs a `GstRTSPMediaFactory` with appsrc; `shared=true`.
- **Control dispatch:** `CommandDispatcher` (`src/app/control/services/dispatcher/`) + `ICommandHandler` (`HandledCases()`/`Handle()`). Handlers registered at `main.cpp:162-189` (the `★ Extensibility point`). `WifiDirectHandler::HandleStart` (`src/app/control/services/handlers/wifi-direct.handler.cpp`) forms the P2P group and already knows `preview_port_` + group-owner IP — the natural place to call `StartAppStream`.
- **Recording/download:** `RecordingService` (`src/app/storage/services/recording_service/`) writes MP4s to `cfg.storage.video`; `DownloadServer` (`src/app/network/services/download_server/`) enumerates MP4s → `RecordingSummary`, mints TTL tokens.
- **Overlay:** `src/{domain,app,adapters}/overlay/` — `OverlayHandler`, cairo renderer, gst compositor; proto→domain mapper at `src/app/overlay/overlay-proto-mapper.cpp`.
- **Build:** `CMakeLists.txt` uses `GLOB_RECURSE` (new `.cpp` auto-picked); `gstreamer-rtsp-server-1.0` already linked. `x264enc` lives in `gstreamer1.0-plugins-ugly`, `rtmp2sink`/`flvmux` in `plugins-bad` — verify on-device and add to `.devcontainer/sysroot/003_install_extra_pkgs.sh` if absent.

### Institutional Learnings

- `docs/solutions/logic-errors/proto-contract-logic-alignment-2026-06-09.md` — overlay semantics are normative: gate proto3 scalars through `has_*()` at the single proto→domain mapper, clamp opacity there (out-of-range alpha poisons the `cairo_t` and silently drops all later draws), uniform aspect-preserving scale + letterbox (never `cairo_scale(sx,sy)`), `push_group` only when `opacity<1.0 || fill.valid`. Run handlers **outside the transport lock** (a full-res encode under `mtx_` froze the control path). Use `system_clock` epoch-ms for wire/wall-clock timestamps. No-skeletons is enforced — `StaticDecision` must be a real deterministic impl, not `kUnimplemented`.
- `sst-cam-proto/docs/solutions/architecture-patterns/cross-stack-contract-drift-2026-06-09.md` — semantic drift is the danger; per-repo tests can't catch it. `camera_index`/`capture_group_id` are identity/routing keys that must mean the same on both ends. Conform overlay composite to `sst-cam-proto/overlay-rendering.md`, not just the cairo code.
- `docs/solutions/tooling-decisions/cpp-rewrite-promotion-to-main-2026-06-09.md` — branch off `main`; keep the proto submodule pinned to the amended contract; ignore `_old/` Python.
- **CLAUDE.md convention:** every new domain model under `src/domain/<m>/models/` ships a `fmt::formatter` (`models/formatter/<model>-fmt.hpp` + `_fmt.hpp` aggregator) in the same change.

### External References

- Jetson Orin Nano has **no NVENC** (NVIDIA "Software Encode in Orin Nano" dev-guide page; forum confirmation). NVDEC (decode) remains. Use `x264enc speed-preset=ultrafast tune=zerolatency`, CBR, `key-int-max≈2×fps`. RidgeRun: ultrafast sustains ~1080p30 at ~30–42% CPU per stream; ~4×1080p30 / ~2×1080p60 parallel is the practical ceiling.
- appsrc: `format=time`, `is-live=true`, set PTS yourself; `block=false` + `leaky-type=downstream` on uplink branches so a stalled RTMP can't stall capture; `gst_buffer_new_wrapped_full` to avoid per-frame copies.
- RTSP: `gst_rtsp_media_factory_set_shared(TRUE)` so all viewers share one encode (critical with software encode). RTMP: `rtmp2sink` (no auto-reconnect — app must rebuild the branch on bus error), `flvmux streamable=true`, silent `audiotestsrc`/AAC track for YouTube.
- Software encoder reads **system memory** — appsrc-fed OpenCV buffers need no NVMM round-trip (drop `nvvidconv`).

---

## Key Technical Decisions

- **Software H.264 (`x264enc`) at all three encode sites:** the Orin Nano has no hardware encoder; `nvv4l2h264enc` will fail. Drop the `nvvidconv`→NVMM hop (software encode wants system memory, which appsrc already provides). **Verify on-device first** (`gst-inspect-1.0 nvv4l2h264enc` / `x264enc`) at M0 before committing tuning. (R3)
- **Keep the existing three-adapter structure initially; consolidate to a single-encode `tee` only if CPU demands it:** smallest change to reach a working demo; the contingency (U9/Deferred) is real because three concurrent software encodes + dual capture + OpenCV is a tight CPU budget. Lower demo resolution/fps (e.g. 720p30 preview) is the first lever.
- **Extract the inline crop into `IDecision` + `StaticDecision`:** the documented intelligence seam; keeps the AI swap to one port. Inject between `slot_.Pop` and `postprocessor_->Process`. (R2)
- **Dual-camera = two producer chains + per-camera `LatestOnlySlot`, decision fan-in:** exercises the real architecture AI will plug into and is required for raw dual capture. (R1)
- **Raw capture taps materialized per-camera frames, writing two files keyed by `capture_group_id` + `camera_index`:** independent lifecycle from final recording (matches the separate `RawCaptureControlCommand` decided in the proto plan). Must consume **materialized** frames so it never pins a GstBuffer past the 5-deep appsink cap.
- **Recording, preview, and RTMP keep encoding even though only cam 0 is chosen:** decision output feeds postprocess once; all final-frame sinks consume the same final buffer (existing `FanOutSink`).

---

## Open Questions

### Resolved During Planning

- Where the decision seam goes: between `LatestOnlySlot.Pop` and postprocess in the consumer (documented extension point).
- Where RTSP starts in production: `WifiDirectHandler::HandleStart`, using group-owner IP + `preview_port_`; `StopAppStream` already fires via `SessionCleanup` on disconnect.
- Raw-capture command shape: separate `RawCaptureControlCommand` (decided in the proto plan).

### Deferred to Implementation

- Whether three concurrent software encodes sustain on-device, or U9's single-encode `tee` consolidation is required — depends on measured CPU. Deferred to on-device execution.
- Raw file container: encoded MP4 (reuses recorder, smaller, needs an encode) vs. raw NV12 segments (no encode, larger, most faithful for YOLO). Resolve against on-device disk/CPU headroom and app labeling needs; coordinate the choice with the app plan's storage metadata.
- Exact demo resolution/fps that fits the CPU budget — tune on-device.

---

## High-Level Technical Design

> *This illustrates the intended approach and is directional guidance for review, not implementation specification. The implementing agent should treat it as context, not code to reproduce.*

```
  cam0 ─ GStreamerAdapter(0) ─ pre ─ materialize ─► LatestOnlySlot<Bundle>0 ┐
                                                                            ├─► ConsumerLoop:
  cam1 ─ GStreamerAdapter(1) ─ pre ─ materialize ─► LatestOnlySlot<Bundle>1 ┘     IDecision.Decide(slot0,slot1)
                    │                                                              → StaticDecision = {cam0, full-frame CropRect}
                    │                                                              → postprocess(chosen) → final buffer
                    │                                                                        │
                    │                                                              FanOutSink ├─► RecordingService ─ x264enc ─ mp4mux ─ file
                    │                                                                        ├─► StreamingService ─┬─ RTSP appsrc ─ x264enc ─ rtph264pay (shared)
                    │                                                                        │                     └─ RTMP appsrc ─ x264enc ─ flvmux ─ rtmp2sink
                    │                                                              (overlay composited onto final frame when enabled)
                    └────────── RawCaptureSink (taps BOTH materialized bundles) ─► file(cam0), file(cam1)  [capture_group_id]
```

Encode element is the change: every `x264enc` box above is `nvv4l2h264enc` today and must become software encode. The dashed RawCaptureSink path is new and independent of the decision/final path.

---

## Implementation Units

### Milestone 0 — On-device bring-up gate (week 1, before any encode work)

Not a code unit — a hard go/no-go run on the real Jetson that de-risks every premise below. Schedule it in **week 1**; if hardware access is intermittent, that scheduling constraint is the dominant project risk.

- Run the **capture** pipeline (`nvarguscamerasrc sensor-id=0/1 ! nvvidconv …`) to PLAYING and pull a frame from **both** sensors — confirms the NVMM/capture stack works on this JetPack build (capture is outside U1's encode scope but the whole demo dies here if it doesn't).
- **Verify the no-NVENC premise, don't assume it:** `gst-inspect-1.0 nvv4l2h264enc` and a trivial `videotestsrc ! nvv4l2h264enc` attempt. If hardware encode actually works, **abandon the software-encode pivot** (U1) and keep `nvv4l2h264enc` — the CPU-budget cascade below disappears.
- Confirm `gst-inspect-1.0 x264enc rtmp2sink flvmux voaacenc` resolve on the on-device rootfs (drives the U1 sysroot edit).
- **Measure CPU** for `2×capture + 1×postprocess-convert + 1×x264enc` before committing to three concurrent encodes — feeds the Phase-5 encode-topology decision (see Risks).

### Phase 1 — Encode fix, decision seam, dual capture, live preview (M1)

- U1. **Software H.264 encode at all three sites**

**Goal:** Recording, RTSP preview, and RTMP encode successfully on the Orin Nano.

**Requirements:** R3

**Dependencies:** None (do first — everything visual depends on encode working)

**Files:**
- Modify: `src/adapters/storage/gstreamer/gst-continuous-recorder.cpp` (+ `.hpp` comment)
- Modify: `src/adapters/streaming/gst_rtsp/gst-rtsp-app-stream-server.cpp` (+ `.hpp` comment; **remove the explicit `GST_BUFFER_PTS/DTS = GST_CLOCK_TIME_NONE`** at ~`:203-205`)
- Modify: `src/adapters/streaming/gst_rtmp/gst-rtmp-streamer.cpp` (+ `.hpp` comment; **`rtmpsink`→`rtmp2sink`** at `:50`, fix `location` to a clean URL — drop the embedded `" live=1"`; **remove `GST_CLOCK_TIME_NONE` PTS/DTS** at ~`:184-186`)
- Modify: `.devcontainer/sysroot/003_install_extra_pkgs.sh` — **unconditional**: add `gstreamer1.0-plugins-ugly` + `libx264-163` (x264enc lives in *ugly*, not the already-installed *bad*) and confirm `voaacenc` (AAC, for U9's silent track) is present
- Test: `tests/streaming/gst_adapters_e2e.test.cpp`, `tests/storage/gst_continuous_e2e.test.cpp` (hardware-bound)

**Approach:**
- Replace `videoconvert ! nvvidconv ! nvv4l2h264enc …` with a software path: `videoconvert ! x264enc speed-preset=ultrafast tune=zerolatency bitrate=<kbps> key-int-max=<2×fps> ! h264parse config-interval=-1 …`. Drop the NVMM hop (software encode reads system memory).
- **Fix timestamping:** `x264enc` with a `format=time` appsrc needs valid PTS — the RTSP/RTMP adapters currently force `GST_CLOCK_TIME_NONE`, which corrupts the software-encoded stream. Either let `do-timestamp=true` stamp on arrival or compute monotonic per-frame PTS; add a test asserting monotonically increasing PTS on output NALs.
- Parameterize bitrate/resolution/fps so the demo can dial CPU down (start 720p30 preview).
- Note: `gst_parse_launch` resolves elements at **runtime**, so a missing `x264enc` keeps the container build green and fails only on-device — the sysroot edit is a hard prerequisite, not a tuning step.

**Execution note:** Verify on-device first against the on-device rootfs (`gst-inspect-1.0 x264enc rtmp2sink flvmux voaacenc`) — these e2e tests are hardware-bound and expected to fail in the container.

**Patterns to follow:** existing adapter pipeline-string construction; `synthetic_frames.hpp` for in-memory frames.

**Test scenarios:**
- Integration (on-device): push synthetic NV12 frames through each adapter; assert a valid H.264 elementary stream / playable MP4 with monotonic PTS is produced.
- Edge case: pipeline construction fails cleanly with a logged error if an encode element is missing (no crash).
- Covers AE2, AE5. Container run: these land in the expected hardware-failure set.

**Verification:** On-device, `gst-inspect-1.0 x264enc` resolves AND each adapter builds to PLAYING and produces a playable H.264 artifact. (Container build being clean is NOT sufficient — runtime element resolution is the real gate.)

---

- U2. **Decision module: `IDecision` port + `StaticDecision`**

**Goal:** The cam-0 full-frame choice becomes a real swappable seam.

**Requirements:** R2

**Dependencies:** None

**Files:**
- Create: `src/domain/decision/models/camera-choice.hpp` (+ `formatter/camera-choice-fmt.hpp`, `_fmt.hpp`)
- Create: `src/app/decision/ports/decision.hpp` (`IDecision::Decide(...) -> CameraChoice`)
- Create: `src/app/decision/services/static_decision/static-decision.{hpp,cpp}`
- Test: `tests/decision/static_decision.test.cpp`

**Approach:**
- `CameraChoice { uint32 camera_index; CropRect crop; }`. `StaticDecision::Decide` ignores inputs, returns `{0, full-frame}` computed from the source geometry.
- Real deterministic implementation (no-skeletons rule). The AI/physics/decision later implements the same port.

**Patterns to follow:** hexagonal port/adapter layout; `fmt::formatter` convention; existing `CropRect`.

**Test scenarios:**
- Happy path: `Decide` returns `camera_index==0` and a crop equal to the full source geometry for various input sizes.
- Edge case: zero/degenerate geometry → defined behavior (clamped, logged), not a crash.

**Verification:** Unit tests green in container; port compiles as a drop-in for the orchestrator.

---

- U3. **Dual-camera orchestration with decision fan-in**

**Goal:** Both cameras run through the orchestrator; the consumer applies `IDecision` before postprocess.

**Requirements:** R1, R2

**Dependencies:** U2

**Files:**
- Modify: `src/app/pipeline/services/orchestrator/pipeline-orchestrator.{hpp,cpp}` (second producer chain + slot; inject `IDecision`; replace inline crop)
- Modify: `src/main.cpp` (construct `GStreamerAdapter(index=1)`, second pre-processor, `StaticDecision`; pass into orchestrator)
- Test: `tests/pipeline/pipeline_orchestrator.test.cpp` (extend the existing `FakeCapture`/`FakePreprocessor`/`FakePostprocessor` harness for two cameras + a fake decision)

**Approach:**
- Add a second producer thread + `LatestOnlySlot<FrameBundle>` for camera 1. `ConsumerLoop` reads the latest from both slots, calls `decision_->Decide(...)`, postprocesses the chosen bundle with the returned crop, pushes to the final `IFrameSink`.
- Materialize each camera's source bundle before its slot push (unchanged rule) so neither pins a GstBuffer.
- Preserve `IFrameSnapshotSource::GrabLatest()` for thumbnails (cached final frame).

**Patterns to follow:** existing two-thread orchestrator; `tests/pipeline/pipeline_orchestrator.test.cpp` fakes pacing ~33ms.

**Test scenarios:**
- Covers AE1. Happy path: both fakes produce frames; assert only cam-0 bundles reach postprocess (decision routes correctly) and cam-1 bundles age out.
- Edge case: one camera stalls (no frames) → consumer still serves cam 0; no deadlock.
- Integration: Start/Stop lifecycle starts/joins both producer threads + consumer cleanly.

**Verification:** Orchestrator tests green; on-device both sensors capture and cam-0 frames flow to sinks.

---

- U4. **Start RTSP preview in production**

**Goal:** Live cam-0 preview is served over WiFi Direct.

**Requirements:** R4

**Dependencies:** U1, U3

**Files:**
- Modify: `src/app/control/services/handlers/wifi-direct.handler.cpp` (call `StartAppStream` after the group is up)
- Modify: `src/app/control/services/handlers/wifi-direct.handler.hpp` — **add an `IStreamingService&` ctor param** (the handler has no streaming member today; this is definite, not conditional)
- Modify: `src/main.cpp` — thread `streaming_service` (already constructed at `:124`, before handler registration at `:174`, so no reordering needed) into the `WifiDirectHandler` ctor
- Test: `tests/control/wifi_direct_handler.test.cpp` — existing test instantiates the handler directly and needs the new ctor arg; fake `IStreamingService` asserting `StartAppStream` is called with the group IP + preview port

**Approach:**
- In `HandleStart`, after `OnWifiReady`, call `streaming_.StartAppStream(AppStreamConfig{address=group->group_owner_ip, port=preview_port_, …})`. `StopAppStream` already runs on disconnect via `SessionCleanup`.
- Keep the call **outside the transport lock** (transport-lock learning).

**Patterns to follow:** `tests/streaming/streaming_handler.test.cpp` fake-service pattern; existing `WifiDirectGroupResponse` fields.

**Test scenarios:**
- Covers AE5 (preview half). Happy path: a `StartWifiDirect` command brings up the group and triggers `StartAppStream` with the correct address/port.
- Error path: `StartAppStream` returns false → handler logs and still returns the group response (preview-degraded, not a hard failure), or returns ERROR — decide and assert one.
- Integration (on-device): an RTSP client renders cam-0 at `rtsp://<group_ip>:8554/preview`.

**Verification:** Phone/RTSP client shows live cam-0 after pairing + WiFi Direct.

---

### Phase 2 — Record + download (M2)

- U5. **Final recording through the new pipeline**

**Goal:** Start/stop recording writes a playable cam-0 MP4; it downloads.

**Requirements:** R5

**Dependencies:** U1, U3

**Files:**
- Verify/modify: `src/app/storage/services/recording_service/*`, `src/adapters/storage/gstreamer/gst-continuous-recorder.cpp`
- Verify: `src/app/network/services/download_server/download-server.cpp` (enumeration + token)
- Test: `tests/storage/*`, `tests/network/*`

**Approach:**
- Confirm `RecordingHandler` START/STOP drives `RecordingService` against the decision-chosen final frames and the software encoder (U1). Confirm `DownloadServer` enumerates the new MP4 and mints a token.
- Mostly validation + any wiring the dual/decision change disturbed; `system_clock` epoch-ms for `started_at`.

**Patterns to follow:** existing recording/download tests.

**Test scenarios:**
- Covers AE3 (record half). Happy path: START → STOP produces an MP4 under `cfg.storage.video`; `Enumerate()` lists it; token validates.
- Edge case: STOP without START, or START while already recording → defined status, no corruption.

**Verification:** On-device, a recorded clip plays back; app can download it.

---

### Phase 3 — Raw dual-camera capture (M3)

- U6. **`RawCaptureControlCommand` handler + raw dual-camera sink**

**Goal:** Independently record both cameras' raw frames to per-camera files.

**Requirements:** R6

**Dependencies:** U3 (cam-1 capturing), proto raw-capture command landed

**Files:**
- Create: `src/app/control/services/handlers/raw-capture.handler.{hpp,cpp}` (`HandledCases() → {kRawCaptureControl}`)
- Create: `src/{domain,app,adapters}/storage/...` raw-capture sink (taps both materialized per-camera bundles) + models (`+ fmt::formatter`)
- Modify: `src/main.cpp` (register handler; wire the raw sink to both producer chains)
- Modify: `src/app/pipeline/services/orchestrator/*` to fork the per-camera materialized bundle to the raw sink **before** the `std::move(*bundle)` into the slot (a `FanOutSink`-style branch — tapping after the move is a use-after-move bug)
- Test: `tests/control/raw_capture_handler.test.cpp`, `tests/storage/raw_capture_*.test.cpp`

**Approach:**
- Handler maps START/STOP to the raw sink. PAUSE/RESUME and unset/`RECORDING_ACTION_UNKNOWN` → `UNSUPPORTED`/`ERROR` (never START). **`capture_group_id` is supplied by the app** on the START command (app-minted, per the proto plan) — firmware stamps both files' metadata with it, does not mint its own. Each camera writes its own file tagged `camera_index` 0/1.
- The raw sink consumes **materialized** frames via its **own bounded queue + dedicated writer thread** — never synchronous file I/O on the capture thread (raw NV12 at 1080p30 is ~93 MB/s per camera; blocking the producer would stall capture). Forking the materialized bundle adds one deep copy per camera per frame — account for it in the M0 CPU budget.
- File format per the deferred decision (raw NV12 segments vs. encoded) — coordinate with the app plan; note the ~186 MB/s two-camera sustained-write load against on-device disk.
- Independent of final recording: both can run concurrently.

**Patterns to follow:** existing handler + `dispatcher.Register` at `main.cpp:~189`; storage recorder structure; identity-key discipline for `camera_index`/`capture_group_id`.

**Test scenarios:**
- Covers AE4. Happy path: START produces two open files; feeding frames writes both; STOP closes both with matching `capture_group_id`.
- Edge case: raw + final recording running together don't interfere (separate sinks).
- Error path: PAUSE/RESUME → `UNSUPPORTED`; START while already raw-capturing → defined status.
- Integration: cam0/cam1 files are frame-aligned enough to label.

**Verification:** On-device, a raw session yields two per-camera files.

---

- U7. **Surface raw recordings for download**

**Goal:** The app can list and download raw files with their identity metadata.

**Requirements:** R6

**Dependencies:** U6

**Files:**
- Modify: `src/app/network/services/download_server/download-server.cpp` (+ `RecordingSummary`/metadata to carry `is_raw`, `camera_index`, `capture_group_id`)
- Test: `tests/network/*`

**Approach:**
- Extend enumeration to include raw files and populate the new `RecordingMetadata` fields (from the amended proto) so the app can group cam0/cam1 by `capture_group_id`.

**Test scenarios:**
- Happy path: a raw session enumerates as two entries sharing `capture_group_id`, distinct `camera_index`, `is_raw=true`; tokens validate.
- Edge case: final recordings still enumerate with `is_raw` absent/false.

**Verification:** App lists raw pairs and downloads both.

---

### Phase 4 — Overlay composite (M4)

- U8. **Overlay composite onto final frames**

**Goal:** Banner/scoreboard composites onto recorded + streamed frames when enabled.

**Requirements:** R7

**Dependencies:** U1, U3, U5

> **Build, not verify.** The overlay module exists (`GstOverlayCompositor`, cairo renderer) but is **not wired into the final-frame production path** — postprocess → `FanOutSink` → storage/streaming has no overlay step today. This unit builds that wiring; it is closer to U3-scale than a validation pass.

**Files:**
- Verify/modify: `src/{domain,app,adapters}/overlay/*`, `src/app/overlay/overlay-proto-mapper.cpp`
- Modify: `src/app/pipeline/services/orchestrator/*` and `src/main.cpp` — thread the overlay output into the final frames before storage/streaming
- Test: `tests/overlay/*` (incl. `gst_overlay_e2e.test.cpp`)

**Approach:**
- **Resolve the topology first:** `GstOverlayCompositor` is an appsrc-backed GStreamer bin; the software-encode pipeline from U1 is also appsrc-fed. Decide whether the compositor bin composes with the new encode topology, or whether a CPU-side RGBA alpha-blend onto the BGR final frame (before `FanOutSink.Push`) is simpler and more CPU-predictable for the demo. Pick before assigning the unit.
- Confirm the overlay renders from app-authored state and composites onto the final frame before storage/streaming when the user enabled it; either output independently toggled.
- Conform to `sst-cam-proto/overlay-rendering.md`: `has_*()` gating + opacity clamp at the mapper, uniform aspect-preserving scale + letterbox, clip-to-bounds text, `push_group` only when needed.

**Patterns to follow:** the overlay rules from `proto-contract-logic-alignment-2026-06-09.md`.

**Test scenarios:**
- Covers AE4 (overlay half). Happy path: overlay enabled → composited frame carries banner+scoreboard; disabled → clean frame.
- Edge case: absent `visible`/`opacity` default to true/1.0 (no vanished overlay); out-of-range opacity clamped (no poisoned cairo context).

**Verification:** On-device, recorded + streamed frames show the overlay when enabled.

---

### Phase 5 — Platform broadcast (M5)

- U9. **RTMP platform streaming (CPU-aware)**

**Goal:** Push the (optionally overlaid) cam-0 stream to an external RTMP platform.

**Requirements:** R8

**Dependencies:** U1, U4

**Files:**
- Modify: `src/adapters/streaming/gst_rtmp/gst-rtmp-streamer.cpp` (software encode from U1; `rtmp2sink`, `flvmux streamable=true`, silent AAC track, leaky uplink queue, bus-error reconnect)
- Verify: `src/app/control/services/handlers/streaming.handler.cpp` (START builds `PlatformStreamConfig{kRtmp, url=destination}`)
- Test: `tests/streaming/*` (hardware/network-bound RTMP test committed, expected to fail in container)

**Approach:**
- Migrate the existing `rtmpsink` to `rtmp2sink` (different element + clean `location` URL, no embedded `" live=1"`); `flvmux streamable=true` (already present) with a **silent AAC track** (`audiotestsrc is-live=true wave=silence ! voaacenc ! flvmux.audio` — both pads must produce timestamped buffers or flvmux stalls). `block=false`/leaky on the uplink branch so a stalled socket can't stall capture; app-level reconnect on bus error (`rtmp2sink` has none).
- **Encode topology — decide from M0 CPU data, not as an afterthought:** three concurrent software encodes (recording + RTSP + RTMP) on top of 2×capture + full-res NV12→BGR + overlay is very likely over a 6-core Orin Nano's budget. The single-encode `tee` is **a probable default, not a safety net** — and it is a real cross-adapter rearchitecture (RTSP's media-factory appsrc, RTMP, and recording are three independent pipelines today; `streaming-service.hpp` notes each destination owns its own encode). If M0 shows three encodes don't fit: first lower resolution/fps (720p30), then collapse to one `x264enc ! tee` feeding all sinks (own a separate plan-step for this; it is not a flip-a-flag contingency). Confirm postprocess can crop **before** the full-res color convert to cut CPU.

**Patterns to follow:** existing `StreamingHandler`/`GstRtmpStreamer`; tee+leaky-queue topology from research.

**Test scenarios:**
- Covers AE5 (RTMP half). Integration (on-device/network): START with a valid RTMP URL+key → platform receives cam-0 video (with overlay when enabled).
- Error path: uplink drop → RTMP branch rebuilds without killing RTSP preview or capture.

**Verification:** Live stream visible on the platform; preview unaffected by uplink hiccups.

---

### Cross-cutting

- U10. **Refresh stale module-status docs**

**Goal:** README/CLAUDE status tables reflect reality (pipeline/storage/streaming/overlay exist; no DB module; Orin Nano software-encode).

**Requirements:** R9 (project hygiene)

**Dependencies:** None — **do this first, before/alongside U1.** The stale CLAUDE.md marks pipeline/storage/streaming/overlay as "Not started" and claims a SQLite/DbSeeder module that doesn't exist; any implementer of U3/U5/U8 who trusts it will rebuild modules that already work. Two files, under an hour, highest confusion-prevention leverage.

**Files:**
- Modify: `CLAUDE.md` (Module status, Database, Pipeline encode note), `README.md` (roadmap/module checklist)

**Approach:** Correct the "not started" rows; note the no-NVENC/software-encode constraint; remove the stale SQLite/DbSeeder claims.

**Test scenarios:** Test expectation: none — documentation only.

**Verification:** Docs match `src/` reality.

---

## System-Wide Impact

- **Interaction graph:** New `IDecision` sits between the per-camera slots and postprocess; `RawCaptureControlCommand` adds a dispatcher registration + a sink tapping both producer chains; `WifiDirectHandler` gains a `StartAppStream` call. `FanOutSink` continues to feed storage + streaming.
- **Error propagation:** Encode-element-missing → logged pipeline-build failure, not crash. RTMP uplink failure isolated from RTSP/capture (leaky queue + branch rebuild). PAUSE/RESUME on raw → `UNSUPPORTED`.
- **State lifecycle risks:** Two producer threads + consumer + raw sink must Start/Stop/join cleanly; materialize-before-buffer rule preserved so nothing pins the 5-deep appsink cap; concurrent final + raw recording write distinct files.
- **API surface parity:** `camera_index`/`capture_group_id` semantics must match the app (identity-key drift). Overlay composite must match `overlay-rendering.md` byte-for-pixel.
- **Unchanged invariants:** Pull model, BLE/WiFi transport, command-dispatch shape, buffer/materialize contracts, proto framing — all unchanged.

---

## Risks & Dependencies

| Risk | Mitigation |
|------|------------|
| **No NVENC on Orin Nano** — existing encode chains fail on target | M0 *verifies* the premise (`gst-inspect nvv4l2h264enc` + trivial encode attempt) before pivoting; if absent, U1 moves all three sites to `x264enc` with the unconditional `plugins-ugly`+`libx264` sysroot edit. Capture path (`nvvidconv`/NVMM) also exercised at M0. Highest priority. |
| **Three software encodes + 2×capture + full-res convert + overlay exceed CPU — likeliest timeline-killer (deferred discovery + structural fix)** | Pull CPU measurement to **M0**, not implementation. Single-encode `tee` is the probable default, not a contingency, and is a cross-adapter rearchitecture — scope it as its own step. Levers: 720p30, crop-before-convert. |
| **Hardware access timing** — all encode/RTSP/RTMP/sensor assertions are unverifiable until on-device | Schedule M0 in week 1; if Jetson access is intermittent, that is the dominant project risk and gates the 3-week deadline. |
| Raw capture pins GstBuffers and starves the appsink cap | Raw sink consumes materialized frames only; covered by buffer learning + tests. |
| RTMP uplink stall freezes preview/capture | Leaky uplink queue, `block=false`, branch-level reconnect; isolate from RTSP. |
| Overlay semantic drift (vanished/black overlays) | Conform to `overlay-rendering.md`; `has_*()` gating + opacity clamp at the mapper; e2e overlay test. |
| Raw file format undecided (encode vs raw NV12) affects CPU/disk + app labeling | Resolve on-device with the app plan; metadata fields are additive either way. |
| Proto raw-capture command must land first | Sequenced: proto plan → firmware. Submodule bump coordinated. |

---

## Documentation / Operational Notes

- Update `CLAUDE.md`/`README.md` (U10). Capture the worker-thread orchestration + GStreamer software-encode lifecycle as a `/ce-compound` learning after landing — it's a documentation gap today and the no-NVENC fix is high-value institutional knowledge.
- On-device bring-up runbook (M0 element check, resolution/fps tuning) should be recorded for the app's integration runbook.

---

## Sources & References

- **Origin document:** `docs/brainstorms/2026-06-10-hardware-demo-firmware-requirements.md`
- System spec: `sst-cam-app/docs/brainstorms/2026-06-10-hardware-demo-system-requirements.md`
- Proto plan (raw-capture contract): `sst-cam-proto/docs/plans/2026-06-10-001-feat-raw-capture-contract-plan.md`
- Learnings: `docs/solutions/logic-errors/proto-contract-logic-alignment-2026-06-09.md`, `docs/solutions/tooling-decisions/cpp-rewrite-promotion-to-main-2026-06-09.md`, `sst-cam-proto/docs/solutions/architecture-patterns/cross-stack-contract-drift-2026-06-09.md`
- Encode sites: `src/adapters/{storage/gstreamer/gst-continuous-recorder,streaming/gst_rtsp/gst-rtsp-app-stream-server,streaming/gst_rtmp/gst-rtmp-streamer}.cpp`
- Orchestrator: `src/app/pipeline/services/orchestrator/pipeline-orchestrator.cpp`; main wiring: `src/main.cpp`
- External: NVIDIA "Software Encode in Orin Nano" dev guide; GStreamer appsrc / gst-rtsp-server / rtmp2sink docs
