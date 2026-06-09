# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

ScoutCamera firmware — C++20 embedded runtime for NVIDIA Jetson Orin Nano (JetPack 6.2.2 / L4T 36.5). Dual IMX477 cameras, GStreamer pipeline, AI sports tracking. `_old/` contains deprecated Python prototypes; ignore it.

## Build

The **only supported build method is cross-compilation from the Dev Container.** Native on-device builds are not supported.

1. Install the **Dev Containers** extension (`ms-vscode-remote.remote-containers`).
2. Open this repo → **"Reopen in Container"** (or `Dev Containers: Reopen in Container`).
   VSCode builds the image automatically (~37 GB JetPack sysroot extraction, ~20 min first time).
3. VSCode opens inside the container — cmake, clangd, and CMake Tools all work.
4. Configure + build:

```bash
cmake --preset debug
cmake --build --preset debug

# Release
cmake --preset release
cmake --build --preset release
```

Binary lands in `build/<preset>/bin/sst_cam_firmware`.

## Tests

Tests are off by default. Use the `test` preset (sets `SST_ENABLE_TESTS=ON` and pulls GTest via Conan):

```bash
cmake --preset test
cmake --build --preset test
ctest --preset test

# Run single test binary directly for faster iteration
./build/test/bin/sst_cam_firmware_tests --gtest_filter="ConfigLoaderTest.*"
```

**Compilation is the first test.** Before declaring any change done, run `cmake --build --preset debug` (or `--preset test`) and ensure it builds clean. If you cannot make the binary build, you have not finished. Do not hand work back as done while the tree is broken.

**Tests are isolated.** A test for module X must spawn its own dependencies (DB, config, GStreamer pipeline, …), not reach into the app's wiring or borrow state from another test. If your test for a recording-service touches the database, it opens its own SQLite instance (`:memory:` or a per-test temp file) and seeds it explicitly. No shared `DbManager` between tests, no fixtures bleeding through, no "the previous test left this row in place." Each test must be runnable in any order, alone, repeatedly.

Tests must verify behaviour end-to-end at the **module** boundary: feed real inputs through the public port and assert the real outputs (a file on disk, a row in DB, a frame on a loopback socket). Mocks are for things outside the module (network endpoints, hardware that isn't on the test machine), not for the module's own collaborators.

**Hardware-bound tests are still written and committed**, even though they can't pass in the dev container. Tests that need the IMX477 sensors, the Jetson NVENC, BlueZ on a real D-Bus, wpa_supplicant against real radio, etc., go in the same `tests/` tree alongside everything else and are expected to fail when run in the cross-compile container. They pass on the device. Don't gate them with `#ifdef` or skip them — write them for real, run them on-device when you have hardware. The container suite shows them as "Failed" and that's expected.

## Linting & formatting

```bash
# Format (Google style, defined in .clang-format)
clang-format -i src/**/*.cpp src/**/*.hpp

# Tidy (checks defined in .clang-tidy — bugprone, performance, modernize, readability, google-*)
clang-tidy -p build/debug src/path/to/file.cpp
```

`compile_commands.json` is exported automatically to `build/<preset>/` and symlinked by clangd.

## Dependencies

**Conan** (portable, via `conanfile.py`): `nlohmann_json`, `spdlog`, `fmt`, `gtest` (test-only).

**System** (JetPack apt / pkg-config): GStreamer 1.0 (`gstreamer-1.0`, `-sdp`, `-app`, `-video`), OpenCV 4.

Conan packages are resolved automatically on `cmake --preset ...` via `cmake/conan_provider.cmake`. Never manually install Conan packages.

## Architecture

Hexagonal (ports & adapters), per-module. Each module lives under both `src/` subtrees:

```
src/
├── domain/<module>/     # Pure C++ — entities, value objects, invariants. No I/O, no heavy libs.
├── app/<module>/        # Use-case services, orchestration. Depends on domain + ports only.
├── adapters/<module>/   # GStreamer, filesystem, OpenCV. Implements ports.
└── <module>/ports/      # Abstract interfaces (pure virtual). Stable contracts between layers.
```

Dependency direction: `adapters → ports/app/domain`, `app → ports/domain`, `domain → nothing external`.

**Forbidden**: `domain/` importing from `adapters/`; ports exposing GStreamer/OpenCV types; utils that import heavy libs and get used by domain.

## Pipeline

The end-to-end intended flow:

```
       per-camera (×2)                                       shared            chosen frame only        parallel        outputs
─────────────────────────────────────────────────────  ┌───────────────┐   ┌──────────────────┐   ┌─────────────┐   ┌──────────┐
                                                       │               │   │                  │   │             │   │ storage  │
cam 0 ─► capture ─► preprocess ─► materialize ─► buf ─►│  AI/tracking  │──►│                  │   │             │ ┌►│          │
                                                       │   (per cam)   │   │                  │   │             │ │ └──────────┘
                                                       │       │       │   │  postprocess     │   │             │ │
                                                       │       ▼       │   │  (crop + zoom    │   │  overlay    │ │
                                                       │   physics ────┼──►│   + resize)      │──►│  (banner /  │─┤
                                                       │   (shared)    │   │                  │   │  scoreboard)│ │
                                                       │       │       │   │                  │   │             │ │ ┌──────────┐
                                                       │       ▼       │   │                  │   │             │ └►│          │
cam 1 ─► capture ─► preprocess ─► materialize ─► buf ─►│   decision    │──►│  ─► final buffer │   │             │   │ streaming│
                                                       │ (which cam +  │   │                  │   │             │   │          │
                                                       │  crop rect)   │   │                  │   │             │   └──────────┘
                                                       └───────────────┘   └──────────────────┘   └─────────────┘
```

Per-stage notes:

- **Capture (per camera)**: one GStreamer pipeline per IMX477. Pull-based `ICaptureFrame::Capture() → std::optional<Frame>`. The appsink is capped at 5 in-flight buffers — anything that pins a `GstBuffer` downstream eats into that cap.
- **Preprocess (per camera)**: raw NV12 `Frame` → `FrameBundle{ source_frame, ai_frame }`. Grayscale/Binary AI paths touch only the Y plane (no color conversion); RGB does NV12→RGB. `source_frame` is a value-copy of the raw — same `owner` shared_ptr, no pixel copy.
- **Materialize**: `sst::buffer::MaterializeFrame(bundle.source_frame)` deep-copies the source planes into an owned `vector<uint8_t>`. After this the GstBuffer is released. Called by the producer right before `buffer.Push`, so nothing past this point pins the GStreamer appsink.
- **Per-camera buffer**: `LatestOnlySlot<FrameBundle>` (drop older bundles on overwrite). Decouples capture cadence from AI cadence.
- **AI / tracking (per camera)**: consumes `ai_frame`; produces field / ball / (eventually) player detections. Unchosen bundles age out via buffer eviction — both frames and detections drop together.
- **Physics (shared)**: consumes detections from both cameras; estimates ball position / trajectory in world coordinates.
- **Decision (shared)**: takes physics output and picks **which camera** and **which `CropRect`** (zoom level + region in source-frame pixels) best frames the action. Hands the chosen `FrameBundle` + crop rect to postprocess.
- **Postprocess** (chosen frame only): NV12→BGR full-res → crop → resize to output resolution → format-convert. Expensive color conversion is paid here, not in preprocess. Pushes into the final buffer.
- **Final buffer**: small bounded queue between postprocess and the output stage.
- **Overlay (parallel)**: runs alongside the final buffer. Produces banner / scoreboard / event-info overlays from user/app state. Storage and streaming composite this on top of the final frame **if the user enabled it**.
- **Storage**: writes the (optionally overlaid) final frames to disk — local recording / archive.
- **Streaming**: sends the (optionally overlaid) final frames over the network — RTSP / HLS / etc.

Storage and streaming consume the same final buffer in parallel; either can be enabled or disabled independently. Capture threads do minimal work and never block on downstream. Buffers are bounded and drop old data. Threads/orchestration: TBD — modules currently expose pure ports; the worker threads that wire them together haven't been built yet.

## Module status

Built and working:

- [x] **Config** — JSON-loaded device/calibration/storage configs (`src/{domain,app,adapters}/config/`).
- [x] **Database** — SQLite + per-entity repositories + `DbSeeder` (`src/{domain,app,adapters}/db/`).
- [x] **Capture** — GStreamer adapter for dual IMX477 (`src/adapters/capture/frame/gstreamer/`).
- [x] **Buffer** — `LatestOnlySlot<T>`, `DropOldestRing<T>`, plus `MaterializeFrame` for releasing GstBuffer-backed frames at the buffer boundary (`src/domain/buffer/`).
- [x] **Network / control** — WiFi and Bluetooth BLE control modules.
- [x] **Processing** — `IPreprocessor` / `IPostprocessor` ports + OpenCV adapter (Grayscale / Binary / RGB AI modes; crop + resize + format-convert post). `FrameBundle`, `CropRect`, `ColorMode` (`src/{domain,app,adapters}/processing/`).

Not started:

- [ ] **Pipeline orchestration** — per-camera worker threads that wire capture → preprocess → materialize → buffer, and the shared threads for AI → physics → decision → postprocess.
- [ ] **AI / tracking** — TensorRT model + adapter; field and ball first, players + jersey numbers later. One inference per camera.
- [ ] **Physics** — ball trajectory / world-coordinate projection from both cameras' detections.
- [ ] **Decision** — picks which camera's frame + crop / zoom rect; hands off to postprocess.
- [ ] **Overlay** — banner / scoreboard / event-info overlays, runs in parallel; storage + streaming composite it on top of the final frame when enabled.
- [ ] **Storage** — local recording / archive of (optionally overlaid) final frames.
- [ ] **Streaming** — network output (RTSP / HLS / etc.) of (optionally overlaid) final frames.
- [ ] **Microphone** — MAX4466 dual-mic integration.

## Database

SQLite at `/var/lib/sst/cam/data.db`. Schema in `db/schema.sql`, migrations in `db/migrations/`. WAL mode, foreign keys on. JSON blobs used for intrinsic matrices, event payloads, and camera arrays. Seeding done in C++ via `DbSeeder` (`src/app/db/services/db_seeder/`).

## Models

Every domain model (struct/class in `src/domain/<module>/models/`) must ship a `fmt::formatter` specialization so it can be logged via `spdlog`/`fmt`. Place formatters in `src/domain/<module>/models/formatter/<model>-fmt.hpp` and re-export them from a sibling `_fmt.hpp` aggregator (see [src/domain/config/models/formatter/](src/domain/config/models/formatter/) and [src/domain/db/models/formatter/](src/domain/db/models/formatter/) for the pattern). When you add or rename a model, add/update its formatter in the same change.

## Style

Google C++ Style Guide. C++20, `CMAKE_CXX_EXTENSIONS ON`. Headers and sources co-located (no separate `include/`). `SST_REPO_ROOT_DIR` macro available at compile time for resolving config paths in tests.

## No skeletons, no TODOs, no stubs

Ship final, working code. **No skeletons.** A class whose method body is `log("not implemented"); return kUnimplemented;` is a skeleton — do not write it, and if you find one, finish it or delete it. A controller whose route handler is "provisional, until the .proto arrives" is a skeleton — wire the real verb today.

The same rule applies to: `// TODO`, `// FIXME`, commented-out CMake blocks, "uncomment when ready" markers, adapter classes whose only contents are `// TODO: bluez wiring`, and ports with no concrete implementation behind them. If a piece of work has to be deferred, do not leave a placeholder in the tree — track it outside the codebase.

If a new feature needs a new system dep, install it in the sysroot via `.devcontainer/sysroot/003_install_extra_pkgs.sh`, link it in `CMakeLists.txt` as `REQUIRED`, and use it. Do not add a header that includes the not-yet-installed library "to be filled in later."

## Adding system dependencies to the JetPack sysroot

The JetPack `targetfs` tarball does not include every Ubuntu package — when a new system dep is needed (e.g. `sdbus-c++`, `gst-rtsp-server`), add the matching Ubuntu jammy `arm64` `.deb` URLs to [.devcontainer/sysroot/003_install_extra_pkgs.sh](.devcontainer/sysroot/003_install_extra_pkgs.sh). The Dockerfile runs `003_install_extra_pkgs.sh` *before* `002_fix_sysroot.sh`, so newly extracted libraries are picked up by the symlink-fix and `.so` linker-stub passes automatically.
