# AGENTS.md

Guidance for coding agents working in **sst-cam-firmware**. For full
architecture, build commands, and the hexagonal module layout, read
[`CLAUDE.md`](CLAUDE.md) — this file mirrors its CI/CD contract and adds the
rules an agent most often trips on.

## Build & test (devcontainer only)

The **only** supported build is cross-compilation from the repo Dev Container,
which is the published compiler image (gcc-13 aarch64 cross toolchain + the
order-sensitive L4T sysroot prep under `compiler-image/sysroot/`) plus editor tools. Native on-device builds are unsupported. Reopen in
Container, then:

```bash
cmake --preset debug   && cmake --build --preset debug     # or release
cmake --preset test    && cmake --build --preset test && ctest --preset test
```

**Compilation is the first test** — never hand work back with the tree broken.
Hardware-bound tests (real IMX477 / NVENC / BlueZ / radio) are written and
committed but are expected to **fail** in the container; they pass on-device.
Do not `#ifdef`-skip them.

## Linting (hard gate)

`tidy` is a **hard CI gate**: `.clang-tidy` promotes every diagnostic to an
error (`WarningsAsErrors: '*'`) and clang-tidy is version-locked to
`clang-tidy-14`. CI is verify-only — fix fixable findings dev-side first:

```bash
scripts/fix.sh          # clang-format + clang-tidy-14 --fix on STAGED C/C++
scripts/fix.sh --all    # whole src/ + tests/ tree
```

`scripts/fix.sh` and CI both source `scripts/tidy-args.sh` (single source of
truth for cross-toolchain flags — they can't drift).

## CI/CD & releasing

PR-gated, Conventional-Commit driven, Git Flow (same model in proto, firmware,
app; levels are tags, not branches):

- **alpha** (`vX.Y.Z-alpha.N`) — every releasable `development` merge; compiler-image cross-build + container `ctest` in isolation (no hardware).
- **beta** (`vX.Y.Z-beta.N`) — `release/X.Y.Z`, cut from `development` at feature freeze; fixes only.
- **rc** (`vX.Y.Z-rc.N`) — after the hardware test (real Jetson + app): `promote-rc.yml` re-tags the newest beta as `rc.1`; later `release/X.Y.Z` merges mint `rc.N`.
- **stable** (`vX.Y.Z`) — merge `release/X.Y.Z → main`; the last rc's verified binary promoted unchanged.

**Two non-negotiables — do not break these:**

1. **Build-in-PR / tag-on-merge** — the aarch64 cross-build (`ci-scripts`/`format`/`tidy`/`test`) is required on `development` / `release/*` PRs.
2. **`main` never builds** — `release.yml` only promotes the already-built,
   **SHA-256-verified** rc binary. Never add a `cmake`/compiler-image build step
   to `release.yml`.

Branch-scoped workflows; the PR gate checks are folded **inside** the alpha/beta
workflows (`pull_request`-gated), so there is **no standalone `ci.yml`**. Each
starts with an `image` job that resolves the compiler image tag
(`compiler-image/VERSION`, built only by `compiler-image.yml`), which
`tidy-shard`/`test`/the cross-build pull — no per-run rebuild; a `changes` job
skips `tidy-shard`/`test` on docs/workflow-only PRs. The `tidy` hard gate is
**sharded** (`tidy-shard` matrix, host-only aggregator `tidy` = the required check):

| Workflow (name) | Trigger | Does |
| --------------- | ------- | ---- |
| `release-alpha.yml` (`release-alpha`) — owns `development` | PR → `development` | `ci-scripts` (shellcheck + tidy-run/install self-tests + proto pin check) + `format` + `tidy` + `test` (the required checks) |
| `release-alpha.yml` | push → `development` (+ dispatch) | `resolve-version` (alpha) → cross-build → `publish-release` `vX.Y.Z-alpha.N` |
| `release-beta.yml` (`release-beta`) — owns `release/**` | PR → `release/**` | same checks |
| `release-beta.yml` | push → `release/**` (+ dispatch) | `resolve-version` (release: beta.N or rc.N) → cross-build → `publish-release` with SHA-256 + install command |
| `promote-rc.yml` (`promote-rc`) | dispatch on `release/X.Y.Z` | newest beta → `rc.1`, same commit and binary |
| `release.yml` (`release`) — owns `main` | push → `main` (+ dispatch) | `promote` last rc → `vX.Y.Z`, verify + re-attach the binary (no build) |

Version math and publishing are the shared actions of
`ScoutSportTechnology/.github` (`resolve-version`, `publish-release`, `promote`,
pinned `@v1.1.0`). Default `GITHUB_TOKEN` only — no PAT/App. The "Release Tags"
ruleset permits creating compliant semver tags and forbids moving/deleting them.

### Branch + commit + tag rules

- `development` is the default branch; target `feat/*` / `fix/*` PRs at it. Do not
  target `main`. `release/X.Y.Z` takes `fix/*` PRs only, after the cut.
- One `release/X.Y.Z` open at a time; the same branch names in proto, firmware
  and app (`feat/<slug>` for a cross-repo change: proto first, then firmware and
  app re-pin `proto/` to the **merged** proto commit).
- `main` is promote-only: no direct push; PR from `release/*` + 1 approval + green checks.
- Tags `v*` are immutable semver (`-alpha.N` < `-beta.N` < `-rc.N` < stable; no delete/move/force-push).
- Use Conventional Commits; the squash-merge subjects drive the alpha bump
  (`feat:` → minor, `fix:`/`perf:` → patch, `type(scope)!:`/`BREAKING CHANGE:`
  → major, but minor while the major is 0; docs/chore-only → **skip**).

### Operational runbooks (maintainer/admin, not agent-run)

- `docs/ci/rulesets.md` — apply the branch/tag rulesets.
- `docs/ci/version-reset-runbook.md` — reset to the `0.1.0-alpha` line.

## Scope guardrails

- CI/CD + docs changes here do **not** touch C++ / CMake / build-system code.
- Proto stays a git submodule; CI must keep `submodules: recursive` and the
  `sst_proto` codegen before tidy/build.
- No database, no NVENC, no skeletons/TODOs/stubs — see CLAUDE.md before adding
  any of these.
