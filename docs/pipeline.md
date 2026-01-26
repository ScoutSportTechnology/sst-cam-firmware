# Pipeline

This document describes the **end-to-end frame flow** through the system, including **buffers** and the **threading model**. It’s the canonical reference for “what happens to a frame” and “where time goes.”

## High-level flow

Capture → Frame Buffer → Synchronization → Pre-process → Metrics → Cross-camera Decision → Post-process → Encode/Output → Control/Config

Each stage is kept as independent as possible. Stages communicate through **ports** and **buffers**, not by reaching into each other’s internals.
```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            MULTI-CAMERA PIPELINE                            │
└─────────────────────────────────────────────────────────────────────────────┘

  Cam0 ──► [T0] CAPTURE ──► (Latest0, size=1) ─┐
                                                 │
  Cam1 ──► [T1] CAPTURE ──► (Latest1, size=1) ─┘
                                                 │
                                                 ▼
         ┌───────────────────────────────────────────────────────────┐
         │  [T2] PRE-PROCESS (per camera)                            │
         │  • colorspace / resize / normalize                        │
         │  • GPU path preferred (NVMM / CUDA)                       │
         │  • outputs: Tensor0, Tensor1                              │
         └────────────────────────────┬──────────────────────────────┘
                                      │
                                      ▼
         ┌───────────────────────────────────────────────────────────┐
         │  [T2] INFERENCE / FEATURE EXTRACTION                      │
         │  • TensorRT / model runtime                               │
         │  • batch=2 or parallel streams                            │
         │  • outputs: Metrics0, Metrics1                            │
         └────────────────────────────┬──────────────────────────────┘
                                      │
                                      ▼
         ┌───────────────────────────────────────────────────────────┐
         │  [T3] CROSS-CAMERA DECISION                               │
         │  • compare Metrics0 vs Metrics1                           │
         │  • hysteresis / smoothing / overlap logic                 │
         │  • selects winner or blend                                │
         └────────────────────────────┬──────────────────────────────┘
                                      │
                                      ▼
         ┌───────────────────────────────────────────────────────────┐
         │  [T3] POST-PROCESS                                        │
         │  • crop / zoom / warp / overlay                           │
         │  • outputs: FinalFrame                                    │
         └────────────────────────────┬──────────────────────────────┘
                                      │
                                      ▼
                              (FinalQ, size=1..3)
                                      │
                                      ▼
         ┌───────────────────────────────────────────────────────────┐
         │  [T4] STREAM / SAVE                                       │
         │  • encode (NVENC when available)                          │
         │  • stream N outputs                                       │
         │  • optional recording                                     │
         └───────────────────────────────────────────────────────────┘
```
## Buffer placement and intent

- `Latest0`, `Latest1`
  Latest-only buffers between capture and processing. Old frames are dropped by design.

- `FinalQ`
  Small bounded queue to isolate encoding / I/O from the main processing path. Size must stay small to avoid latency creep.

---

## Thread responsibilities

- **[T0], [T1] – Capture threads**
  - One per camera
  - Must never block on downstream stages
  - Minimal work: acquire, timestamp, publish

- **[T2] – Processing thread**
  - Owns pre-process and inference
  - Pulls latest frames from each camera buffer
  - Designed to run at “best effort” real-time speed

- **[T3] – Decision / post-process**
  - May share thread with T2 or be split if needed
  - Contains pure logic + lightweight transforms

- **[T4] – Output thread**
  - Handles encode, stream, file I/O
  - Isolated so backpressure never reaches capture

---

## Real-time invariants

- Capture never waits for processing
- Buffers are bounded and drop old data
- No unbounded queues in the hot path
- Encode / output is always isolated from capture

---

## Core data objects

### Frame (concept)
A frame is the unit of work that moves through the pipeline.

A `Frame` should carry at least:
- `camera_id` (source identifier)
- `timestamp` (monotonic or derived; see timing rules)
- `sequence` (per-camera incrementing counter if available)
- pixel data (CPU memory and/or GPU/zero-copy handle depending on backend)
- metadata (optional: exposure, gain, calibration info, etc.)

Ownership rule:
- A frame is **owned by exactly one stage at a time**.
- Transfer between stages happens by moving pointers/handles through buffers.

### FrameSet (concept)
When multiple cameras are used, the synchronization stage produces a paired/grouped object:
- `FrameSet = { frame(cam0), frame(cam1), ... }`
- A `FrameSet` represents “the best matched group” under the sync policy.

---

## Buffers

Buffers are the mechanical joints of the pipeline. They exist to:
- decouple producer/consumer rates
- prevent blocking hot threads (capture)
- enforce drop policy under load

### Latest-only buffer (recommended for real-time)
For stages where **freshness matters more than completeness**, we use a **latest-only** buffer.

Behavior:
- Producer writes the newest frame.
- If consumer is slow, old frames are overwritten/dropped.
- Consumer always reads the most recent available frame (or waits until one exists).

Use cases:
- Capture → processing when you only care about “what’s happening now”
- UI preview feeds
- Real-time decision logic

Key properties:
- bounded memory
- stable latency (no backlog growth)
- deterministic “drop old” behavior

### Queue/ring buffer (optional for “must not drop”)
For stages where you must not drop data (recording, audit, offline processing), a bounded queue can be used.

Behavior:
- Producer pushes frames to queue.
- Consumer pops in order.
- When full, policy must be explicit: block, drop-oldest, or drop-newest.

This project defaults toward real-time behavior, so queues are used sparingly and only where the product requires it.

---

## Threading model

The pipeline is explicitly multi-threaded. Threads exist so capture stays real-time and processing can scale.

### Capture threads (one per camera)
- Each camera runs capture work on its own thread (or is driven by its backend callback thread).
- The capture thread must do **minimal work**:
  - acquire frame from backend
  - timestamp it
  - push into the capture buffer (latest-only)
- Capture thread must avoid:
  - expensive allocations
  - blocking calls
  - heavy conversions (unless unavoidable at source)

### Processing thread(s)
- A processing thread pulls the latest frames from buffers and runs the main pipeline stages.
- If multi-camera:
  - processing thread reads from each camera buffer
  - synchronization produces a `FrameSet`
  - downstream stages operate on `FrameSet`

Scaling options:
- Start with a single processing thread for determinism and simplicity.
- Add per-stage workers only if profiling proves it’s needed.

### Output/encoding thread (optional)
- Encoding and output can be expensive or block on I/O.
- If encoding blocks the main processing thread, move encode/output to its own thread with a buffer boundary.

### Control/config thread (or main thread)
- Handles runtime control events (start/stop, switch sources, reload config).
- Must coordinate with running threads safely.

---

## Stage-by-stage detail

## 1) Capture
Purpose:
- Acquire frames from camera backends.

Typical implementation:
- `capture/adapters/` talks to GStreamer (or other backend).
- `capture/app/` owns lifecycle (start/stop, error recovery).
- Frames are pushed into a per-camera **LatestFrameBuffer**.

Output:
- `Frame` objects written into `LatestFrameBuffer(camera_id)`.

---

## 2) Frame Buffer (latest-only)
Purpose:
- Provide fast handoff from capture to processing without backlog.

Policy:
- Overwrite old frames when producer runs ahead.
- Consumer reads most recent frame.

Notes:
- If timestamps are generated at capture, they must remain attached to the frame through the pipeline.
- Buffer operations should be lock-free or minimal-lock (fast path).

---

## 3) Synchronization / Pairing (multi-camera)
Purpose:
- Produce a coherent `FrameSet` across cameras.

Inputs:
- Latest frames from each camera buffer.

Output:
- `FrameSet` that best satisfies the sync policy.

Sync policy (typical):
- Define a max skew tolerance (e.g., Δt <= X ms).
- Prefer the newest set that satisfies tolerance.
- If no valid set exists:
  - either wait briefly for better match
  - or emit partial set (if allowed)
  - or drop and retry (real-time default)

This stage is where multi-camera systems live or die. Keep rules explicit and documented.

---

## 4) Pre-process / Feature extraction
Purpose:
- Convert frames into the representation required for inference/tracking/decision.

Typical operations:
- colorspace conversion
- resize/crop
- normalization
- feature extraction hooks (optional)

Performance rule:
- Avoid copies. Prefer reusing buffers and doing in-place transforms when safe.

Output:
- processed frame(s) or features attached to `Frame` / `FrameSet`.

---

## 5) Metrics / Tracking / Smoothing
Purpose:
- Reduce jitter and noise in measurements across time.

Examples:
- FPS / latency tracking
- temporal smoothing (EMA, windowed median, etc.)
- object tracking state (if present)

Notes:
- Keep state ownership clear (per-camera vs global).
- Metrics should not block the hot path.

Output:
- updated state + metrics for downstream stages.

---

## 6) Cross-camera decision
Purpose:
- Fuse information across cameras and make a single decision output.

Inputs:
- `FrameSet` and derived features/metrics.

Output:
- a decision object (domain type), e.g.:
  - selected camera feed
  - event triggers
  - confidence values

Rules:
- Decision logic belongs in `domain/` (pure) with inputs as plain types.
- Any I/O or backend queries happen via ports.

---

## 7) Post-process / Composition
Purpose:
- Prepare final visual output or artifacts.

Examples:
- overlay annotations
- compose multiple frames (side-by-side, picture-in-picture)
- final colorspace conversion for encoder/output

Output:
- final `Frame` (single) ready to encode/output.

---

## 8) Encode / Output
Purpose:
- Deliver results (file, stream, UI sink, network).

Notes:
- Encoding can block; isolate behind a buffer/thread if needed.
- Output is adapter territory (filesystem, GStreamer sink, etc.).

---

## 9) Control / Configuration
Purpose:
- Manage lifecycle and runtime changes.

Examples:
- start/stop capture
- switch device model/pipeline preset
- reload config
- adjust sync tolerance

Rules:
- Control must coordinate thread shutdown/startup deterministically.
- Avoid “half-applied” config. Prefer atomic swap of config snapshots.

---

## Backpressure and drop behavior

This system is real-time oriented.

Default policies:
- Capture never blocks on processing.
- Latest-only buffers drop old frames under load.
- Synchronization prefers freshness; if pairing fails repeatedly, it should degrade gracefully (explicit policy).

If a “must-not-drop” recording mode is added later, it should be a separate pipeline path with explicit queueing and storage guarantees.

---

## Practical invariants (non-negotiable)

- Capture threads do minimal work and never block on downstream.
- Buffer ownership is explicit; no shared mutable frame data across stages.
- Sync policy is explicit: tolerance, fallback behavior, and logging.
- Encoding/output is isolated if it can block.
