# CI/CD & Release Pipeline — Requirements (sst-cam-firmware)

**Date:** 2026-06-10
**Status:** Ready for planning
**Type:** Standard — infrastructure / DevOps
**Cross-repo note:** Part of a shared CI/CD effort across sst-cam-app, sst-cam-firmware, sst-cam-proto. This doc covers the **firmware** slice; governance below is duplicated in each repo because it applies to each.

## Problem

No automated CI or release flow. `main` unprotected, binaries cross-built and copied to the Jetson by hand. Need lint → test → release automation with protected `main`, semver releases, and a repeatable Jetson install.

## Goals

- `main` default, PR-gated, immutable semver tags.
- PR runs **lint + test** in the devcontainer cross-build env; merge blocked until green.
- Automated semver releases (no hand-cut tags).
- Release artifact = **production** Jetson binary on GitHub Releases.
- Repeatable manual install/update on the Jetson via `install.sh`.

## Non-Goals (now)

- Full OTA (A/B rootfs, Mender/RAUC/balena) — revisit at fleet scale.
- Auto-pull (systemd timer) or CI-push updates — manual run only for demo phase.
- Debug/developer binary release — production only for now.

---

## Branch & Tag Governance (shared, applies here)

### Branch ruleset (`main`)
- `main` = default; feature branches unrestricted, per-developer.
- Require PR; **1 approval**; dismiss stale reviews on push; require thread resolution.
- Block force-push (`non_fast_forward`) + deletion.
- **Add `required_status_checks`** = this repo's lint+test jobs, once workflows exist.

### Tag ruleset (`v*`)
- Semver regex: `^v\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?(\+[0-9A-Za-z.-]+)?$`.
- Immutable: block `update`, `deletion`, `non_fast_forward`.
- **Bypass actors:** `OrganizationAdmin` + the org **GitHub App** (added via GitHub UI, not JSON — UI resolves the App name to its numeric `actor_id`; a string id fails import with "invalid actor"). This is the existing `release-tags.json` for this repo; remove any hand-added string actor before import and add the App in the UI.

### Release mechanism (shared)
- **release-please** (PR-based): conventional commits on `main` → release PR → merge bumps semver, tags, cuts GitHub Release.
- **Identity:** one org-owned **GitHub App**, installed on all 3 repos; its token pushes tags and is the tag-ruleset bypass actor. Chosen over `GITHUB_TOKEN` because the default token's recursion guard means its tag push would **not** trigger the release-build workflow.
- Requires conventional-commit discipline on `main`.

---

## Firmware-Specific Requirements

- **PR CI:** lint + tests inside the existing **devcontainer cross-build environment** (conan-cache mount + FindProtobuf reconfigure gotchas already known — see firmware build env notes).
- **Release (on release-please tag):** cross-build the **production** binary only → upload as GitHub Release asset.
- **Proto:** keeps the **git submodule** (C++ codegen at build time). No SDK consumption here — a C++ SDK is out of scope. proto still serves app via SDK and firmware via submodule.

### Install / update — `install.sh` (manual)
Operator runs it by hand on the Jetson when they want to update:
1. `curl` the latest GitHub Release asset (auth needed for private-repo asset download).
2. Stop the systemd service.
3. Swap the binary.
4. Restart the service.

- Manual run only — no auto-pull timer, no CI push, for the demo phase.
- Idempotent; safe to re-run; handles "already latest" gracefully.

## Success Criteria

- PR to `main` can't merge until lint + test pass and 1 review approves.
- Tags only exist if semver; releases immutable.
- Merging release-please PR cuts a Release with the prod Jetson binary, no manual tagging.
- `install.sh` updates a Jetson to the latest release in one manual run.
- Firmware still builds against the proto submodule unchanged.

## Dependencies / Assumptions

- CI = GitHub Actions; devcontainer cross-build runs in CI.
- Org GitHub App created + installed + added as bypass actor (org-admin task).
- Conventional commits on `main`.
- Jetson runs systemd and is reachable by the operator.
- Jetson can authenticate to download a private-repo Release asset.

## Open Questions (for planning)

- How the Jetson authenticates to pull a private Release asset (token on device vs PAT vs deploy key).
- systemd service name + binary path on the Jetson (for `install.sh`).
- `buf`/proto-submodule update cadence in firmware builds.
