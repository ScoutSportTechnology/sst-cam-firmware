# ROADMAP

This file tracks **project progress** and **next steps**.
It is intentionally practical: no design discussion, no theory, just what exists and what still needs to be built.

This roadmap reflects the **real-time, multi-camera pipeline** described in `docs/pipeline.md`.

---

## 0. Project Baseline

- [x] Repository structure finalized
- [x] Hexagonal architecture defined
- [x] Single `src/` tree (hpp + cpp co-located)
- [x] Google C++ style adopted
- [x] Docs structure created (`docs/`)

---

## 1. Configuration

Status: **DONE**

- [x] Config domain models
- [x] Device model selection
- [x] Validation
- [x] Runtime access from app layer

Notes:

- Config is treated as immutable snapshots at runtime.

---

## 2. Capture

Status: **DONE**

- [x] Capture ports defined
- [x] GStreamer adapter implemented
- [x] Device discovery
- [x] Frame acquisition
- [x] Timestamp assignment
- [x] Smoke tests for pipeline startup

Notes:

- Capture threads must never block downstream.

---

## 3. Frame Buffers

Status: **IN PROGRESS**

- [ ] Latest-only frame buffer (size = 1)
- [ ] Lock-free or minimal-lock implementation
- [ ] Explicit ownership transfer semantics
- [ ] Clean shutdown behavior

Notes:

- Default policy: drop old frames.
- No unbounded queues allowed in hot path.

---

## 4. Frame Synchronization / Pairing

Status: **TODO**

- [ ] Sync policy definition (max skew tolerance)
- [ ] FrameSet domain type
- [ ] Best-match selection logic
- [ ] Partial / missing frame handling
- [ ] Instrumentation for sync misses

Notes:

- Sync logic must be explicit and deterministic.

---

## 5. Pre-Process

Status: **TODO**

- [ ] Colorspace conversion
- [ ] Resize / normalize
- [ ] GPU fast path (NVMM / CUDA when available)
- [ ] Zero-copy rules documented

Notes:

- No silent CPU fallbacks without logging.

---

## 6. Inference / Feature Extraction

Status: **TODO**

- [ ] Inference port definition
- [ ] TensorRT adapter
- [ ] Multi-camera batch or stream support
- [ ] Output metrics domain types

Notes:

- Inference must not own camera-specific state.

---

## 7. Metrics / Tracking / Smoothing

Status: **TODO**

- [ ] Temporal smoothing (EMA / windowed)
- [ ] FPS and latency metrics
- [ ] Confidence tracking
- [ ] Debug visibility hooks

Notes:

- Metrics must not block the hot path.

---

## 8. Cross-Camera Decision

Status: **TODO**

- [ ] Decision domain model
- [ ] Comparison logic (score, confidence, overlap)
- [ ] Hysteresis rules
- [ ] Stability guarantees (no rapid flip-flop)

Notes:

- Decision logic must be pure and testable.

---

## 9. Post-Process

Status: **TODO**

- [ ] Crop / zoom
- [ ] Warp / overlay
- [ ] Final frame composition

Notes:

- Operates on a single selected frame or blend.

---

## 10. Encode / Output

Status: **TODO**

- [ ] Output ports
- [ ] NVENC adapter
- [ ] Stream output
- [ ] Recording output
- [ ] Small bounded output queue (size 1–3)

Notes:

- Output must never apply backpressure to capture.

---

## 11. Control / Runtime Configuration

Status: **TODO**

- [ ] Start / stop lifecycle
- [ ] Graceful shutdown
- [ ] Runtime parameter updates
- [ ] Error propagation strategy

Notes:

- Config changes should be atomic.

---

## 12. Observability & Debug

Status: **TODO**

- [ ] Structured logging per stage
- [ ] Frame counters
- [ ] Drop-rate visibility
- [ ] Optional frame dumping (debug only)

---

## 13. Performance & Hardening

Status: **TODO**

- [ ] Latency budget per stage
- [ ] Memory allocation audit
- [ ] Thread affinity (if needed)
- [ ] Stress tests under load

---

## Guiding principles

- Real-time > completeness
- Bounded memory everywhere
- Explicit ownership and drop policies
- No blocking in capture threads
- No “magic” background behavior
