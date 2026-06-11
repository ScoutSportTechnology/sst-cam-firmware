---
title: "feat: CI/CD & release pipeline (sst-cam-firmware)"
type: feat
status: active
date: 2026-06-10
origin: docs/brainstorms/ci-cd-release-pipeline-requirements.md
---

# feat: CI/CD & Release Pipeline (sst-cam-firmware)

**Target repo:** sst-cam-firmware (C++ / CMake+Conan, Jetson aarch64 cross-build)

## Summary

Add GitHub Actions CI that runs the existing devcontainer cross-build with clang-format/clang-tidy + GTest (under qemu) as the merge gate, a release-please-driven release workflow that cross-builds the production `sst_cam_firmware` binary and attaches it to the GitHub Release, and a manual `install.sh` + systemd unit so an operator can update a Jetson in one command. Proto stays a submodule.

---

## Problem Frame

Firmware builds today only run manually inside the devcontainer via `justfile`; there is no CI, no release artifact, and no install path on the Jetson (no systemd unit, no installer). Cross-compilation is non-trivial (custom toolchain, sysroot prep, host-protoc codegen, OpenCV SONAME pinning), so CI must reuse the devcontainer rather than reinvent the build.

---

## Requirements

- R1. PRs to `main` run clang-format check + clang-tidy + GTest in the cross-build env; merge blocked until green (1 approval).
- R2. Release-please drives semver tags/releases from conventional commits on `main`.
- R3. On a release tag, cross-build the **production** `sst_cam_firmware` binary and upload it as a GitHub Release asset.
- R4. A manual `install.sh` updates a Jetson: fetch the release asset → stop service → swap binary → restart, idempotently.
- R5. Proto submodule consumption keeps working unchanged.

**Origin actors:** developers (PRs), operator (runs `install.sh` on the Jetson).

---

## Scope Boundaries

- No full OTA (A/B rootfs, Mender/RAUC/balena) — manual installer only.
- No auto-pull (systemd timer) and no CI-push to the device.
- Production binary only — no debug/emulated release artifact.
- Proto stays a submodule; no C++ SDK.

### Deferred to Follow-Up Work

- Org GitHub App creation: shared prerequisite across the 3 repos (see Risks & Dependencies).

---

## Context & Research

### Relevant Code and Patterns

- Build: `CMakeLists.txt` (3.24+, C++20), `conanfile.py`, `CMakePresets.json` (presets `debug`/`release`/`test`), `cmake/conan_provider.cmake`.
- Cross-compile: `cmake/toolchains/jetson-r36.5.cmake`, `conan/profiles/cross` + `conan/profiles/jetson-r36.5-cross`, sysroot `CROSS_SYSROOT=/l4t/targetfs`.
- Devcontainer: `.devcontainer/devcontainer.json`, `.devcontainer/docker-compose.yml`, base `nvcr.io/nvidia/jetpack-linux-aarch64-crosscompile-x86:6.1`, sysroot scripts `.devcontainer/sysroot/00{1,2,3}_*.sh` (order-sensitive). Conan cache bind-mounted at `~/.conan2`.
- Build/test runner: `justfile` (`build-release`, `build-test`, `test`, `ctest`); tests run under `QEMU_LD_PREFIX=/l4t/targetfs`.
- Targets: executable `sst_cam_firmware` → `build/<preset>/bin/`; tests `sst_cam_firmware_tests` (GTest, `gtest_discover_tests`).
- Lint: `.clang-format` (Google), `.clang-tidy`; `compile_commands.json` per preset.
- Proto: `.gitmodules` (`proto/` submodule); host-protoc codegen in `CMakeLists.txt` (lines ~130–183) with documented FindProtobuf/multiarch gotchas in `CLAUDE.md`.

### Institutional Learnings

- `CLAUDE.md` cross-build gotchas: OpenCV SONAME pin (408), reset `Protobuf_PROTOC_EXECUTABLE` to host before `find_package`, strip protobuf `INTERFACE_INCLUDE_DIRECTORIES`, sysroot script ordering, conan-cache persistence. CI must preserve all of these — i.e. build through the existing devcontainer image.

### External References

- `devcontainers/ci` GitHub Action (run steps inside the repo's devcontainer) — reuses the exact build env and caches the image.
- GitHub Release asset download with auth (private repo) for `install.sh` (token + `curl -H "Authorization: Bearer"` against the asset API).

---

## Key Technical Decisions

- **CI runs inside the existing devcontainer**, not a hand-rolled toolchain install. The cross-build's correctness depends on the sysroot prep + image; reproduce it via `devcontainers/ci` (or `docker compose run`) so gotchas stay solved in one place.
- **Cache the heavy bits**: the NVIDIA base image layer and the `~/.conan2` cache, keyed on `conanfile.py` — sysroot unpack is the slow step, so persist `/l4t/targetfs` where feasible.
- **Tests run under qemu** exactly as `justfile` does (`QEMU_LD_PREFIX`), via `ctest --preset test`.
- **install.sh is dumb and idempotent**: version-check, atomic binary swap, systemd restart, safe re-run. systemd unit shipped alongside.
- **GitHub App token** for release-please so the tag triggers the release build and satisfies tag-ruleset bypass.

---

## Open Questions

### Resolved During Planning

- CI build mechanism: reuse devcontainer image via CI action.
- Release variant: production only.

### Deferred to Implementation

- Whether the GH-hosted runner can unpack the L4T sysroot within time/disk limits, or whether a self-hosted runner is needed — measure first build; fall back to self-hosted if the sysroot step is too heavy.
- systemd service name + install path on the Jetson (proposed `/opt/sst-cam/bin/sst_cam_firmware`, unit `sst-cam-firmware.service`) — confirm with the device's existing layout.
- How the Jetson authenticates to download a private Release asset (device PAT vs deploy token) — operator/security decision.

---

## Implementation Units

### U1. PR CI workflow (format + tidy + tests in devcontainer)

**Goal:** Gate PRs on clang-format, clang-tidy, and GTest via the cross-build devcontainer.

**Requirements:** R1, R5

**Dependencies:** None

**Files:**
- Create: `.github/workflows/ci.yml`
- Possibly add: a thin `justfile` recipe (e.g. `ci-check`) that bundles format-check + tidy + build-test + ctest for CI to call.

**Approach:**
- Use `devcontainers/ci` to build/start the repo devcontainer, then run: `clang-format --dry-run --Werror` over `src/`+`tests/`; `clang-tidy -p build/<preset>` on changed sources; `cmake --preset test && cmake --build --preset test && ctest --preset test` (qemu).
- Cache the devcontainer image and `~/.conan2`. Keep proto submodule checked out (`submodules: recursive`).
- Job name(s) become `required_status_checks` (U5).

**Test scenarios:**
- Test expectation: none — CI config; validated by a trial PR: clean PR green; a format violation fails the format job; a failing GTest fails the test job.

**Verification:**
- A PR with a failing unit test or unformatted code cannot merge; proto submodule resolves in CI.

### U2. Release workflow — cross-build + upload binary

**Goal:** On a release tag, cross-build the production binary and attach it to the Release.

**Requirements:** R2, R3, R5

**Dependencies:** U1, U4 (installer expects asset name), U5

**Files:**
- Create: `.github/workflows/release.yml`

**Approach:**
- Trigger on the release-please tag (App token so it fires). In the devcontainer: `cmake --preset release && cmake --build --preset release`.
- Upload `build/release/bin/sst_cam_firmware` as a Release asset with a stable, versioned name (e.g. `sst_cam_firmware-<ver>-aarch64`).

**Test scenarios:**
- Test expectation: none — release automation; validated by a tagged dry run producing an aarch64 binary asset (`file` reports ARM aarch64).

**Verification:**
- A release tag yields a Release carrying the production aarch64 binary; `file` confirms the architecture.

### U3. Manual installer + systemd unit

**Goal:** Ship `install.sh` and a systemd unit so an operator updates a Jetson in one run.

**Requirements:** R4

**Dependencies:** U2 (asset name/format)

**Files:**
- Create: `deploy/install.sh`
- Create: `deploy/sst-cam-firmware.service`
- Create/Modify: `deploy/README.md` (one-time setup + update usage)

**Approach:**
- `install.sh`: resolve latest (or `--version vX.Y.Z`) Release asset via the GitHub API (authenticated for private repo) → download to a temp path → verify it's executable/aarch64 → `systemctl stop sst-cam-firmware` → atomically move into `/opt/sst-cam/bin/` → `systemctl start` → print active status. Idempotent: re-running on the current version is a no-op (skip swap when checksum/version matches).
- systemd unit: simple service, `ExecStart` the installed binary, `Restart=on-failure`, appropriate `WorkingDirectory`/user.

**Execution note:** This is the one behavior-bearing unit — test the script's branch logic (already-latest, stop/start, rollback-on-failure) explicitly.

**Test scenarios:**
- Happy path: from a clean device, `install.sh --version vX.Y.Z` downloads, installs to `/opt/sst-cam/bin/sst_cam_firmware`, and the service reaches `active (running)`.
- Edge case: re-running with the same version is a no-op (no needless restart).
- Error path: a failed download or non-executable asset aborts **before** stopping the running service (no downtime on bad fetch).
- Error path: if the new binary fails to start, surface the failure clearly (and, if feasible, keep/restore the previous binary).
- Edge case: missing auth token → clear error, not a silent empty download.

**Verification:**
- One `install.sh` run updates a Jetson to a chosen release and leaves the service running; bad inputs fail safe without taking the service down.

### U4. release-please config

**Goal:** Conventional-commit-driven semver releases for firmware.

**Requirements:** R2

**Dependencies:** None

**Files:**
- Create: `release-please-config.json` (release-type `simple`; optionally bump `conanfile.py` version via extra-files)
- Create: `.release-please-manifest.json`
- Create: `.github/workflows/release-please.yml`

**Approach:**
- release-please maintains the release PR; merge tags `vX.Y.Z` and cuts the Release that U2 builds into. Auth via org GitHub App token.

**Test scenarios:**
- Test expectation: none — validated by a dry-run release PR producing the expected bump (and matching `conanfile.py` version if wired).

**Verification:**
- Merging a `feat:` release PR tags + releases; U2 fires on the tag.

### U5. Branch & tag governance

**Goal:** Wire CI as required check; the existing rulesets get the App bypass actor.

**Requirements:** R1

**Dependencies:** U1

**Files:**
- Reference (repo settings): existing `Default.json` branch ruleset + `release-tags.json` tag ruleset (this repo already has them).

**Approach:**
- Add `required_status_checks` = U1 job name(s) to the branch ruleset.
- Tag ruleset: add the org GitHub App as a bypass actor **via the GitHub UI** (the earlier JSON import failed precisely because a string `actor_id` was hand-added — import the clean `release-tags.json`, then add the App in the UI).

**Test scenarios:**
- Test expectation: none — repo settings; validated by a blocked direct push to `main` and a blocked non-semver tag.

**Verification:**
- CI required before merge; only App/admin can push `v*` tags; clean `release-tags.json` imports without the "invalid actor" error.

---

## System-Wide Impact

- **Interaction graph:** CI must check out the proto submodule recursively or the build's codegen step fails.
- **State lifecycle risks:** `install.sh` swapping a live binary — stop-before-swap and fail-before-stop ordering prevent a half-updated running service.
- **Operational:** introduces a systemd-managed service on the Jetson where there was none — document the one-time unit install.
- **Unchanged invariants:** CMake presets, toolchain, sysroot prep scripts, proto submodule path `proto/`, and all documented cross-build gotchas remain as-is.

---

## Risks & Dependencies

| Risk | Mitigation |
|------|------------|
| GH-hosted runner can't unpack the L4T sysroot (time/disk) | Measure on first run; fall back to a self-hosted runner that keeps `/l4t/targetfs` warm. |
| Devcontainer image pull is slow each run | Cache the image layer + `~/.conan2`; key conan cache on `conanfile.py`. |
| Private-repo asset download auth on the Jetson | Resolve token strategy before first install (deferred Open Question); `install.sh` must fail clearly when auth is missing. |
| Live-service swap causes downtime/breakage | Fail-before-stop on bad fetch; idempotent no-op on same version; surface start failures. |
| `GITHUB_TOKEN` tag won't trigger U2 | Use the org GitHub App token in release-please (U4). |
| Org GitHub App not created | Shared prerequisite; block U4 wiring until provisioned. |

---

## Sources & References

- **Origin document:** docs/brainstorms/ci-cd-release-pipeline-requirements.md
- Related code: `CMakeLists.txt`, `CMakePresets.json`, `conanfile.py`, `cmake/toolchains/jetson-r36.5.cmake`, `.devcontainer/`, `justfile`, `.clang-tidy`, `CLAUDE.md`, `Default.json`, `release-tags.json`
- Cross-repo: `sst-cam-proto/docs/plans/2026-06-10-001-feat-ci-cd-release-pipeline-plan.md`, `sst-cam-app/docs/plans/2026-06-10-001-feat-ci-cd-release-pipeline-plan.md`
