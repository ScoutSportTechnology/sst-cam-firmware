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

Capture (T0/T1) → Latest-only buffer → Pre-process + Inference (T2) → Cross-camera decision (T3) → Post-process → FinalQ → Encode/Stream (T4).

Capture threads do minimal work and never block on downstream. Buffers are bounded, drop old data. Encoding is isolated behind a thread boundary.

## Database

SQLite at `/var/lib/sst/cam/data.db`. Schema in `db/schema.sql`, migrations in `db/migrations/`. WAL mode, foreign keys on. JSON blobs used for intrinsic matrices, event payloads, and camera arrays. Seeding done in C++ via `DbSeeder` (`src/app/db/services/db_seeder/`).

## Style

Google C++ Style Guide. C++20, `CMAKE_CXX_EXTENSIONS ON`. Headers and sources co-located (no separate `include/`). `SST_REPO_ROOT_DIR` macro available at compile time for resolving config paths in tests.
