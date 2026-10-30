# Branch and tag rulesets — sst-cam-firmware

The active GitHub branch rulesets for `development`, `release-branches` and `main` require the `lint`, `compile` and `test` job contexts from `.github/workflows/ci.yml`. Confirm the live rulesets before changing required checks; do not change repository settings as part of a firmware PR.

| Target | Gate |
|---|---|
| `development` | PR with `lint`, `compile`, `test`; no force-push or delete. |
| `release/**` | PR with `lint`, `compile`, `test`; no force-push or delete. |
| `main` | PR, approval and `lint`, `compile`, `test`; no direct push, force-push or delete. Admin bypass is reserved for approved promotion. |
| `v*` tags | Immutable Release Tags ruleset; workflows create tags, not developers. |

`ci.yml` runs on PRs to all three branch targets. For a `main` PR, `lint` runs, `test` asserts lint succeeded and `compile` is skipped (skipped jobs satisfy the required context); `main` never rebuilds the promoted binary. For docs-only PRs to other targets, the path filter skips the expensive jobs. The workflow's `image` job verifies the pinned compiler image before code jobs; `publish.yml` uses the same pinned image to build the release asset. Only `compiler-image.yml` publishes the compiler image.

`release.yml` publishes alpha/beta/rc on `development` and `release/**` pushes through `publish.yml`; on a `main` push it only promotes the last rc, verifying its SHA-256 before publishing the stable release. `Release Tags` must continue to allow compliant workflow-created `v*` tags without allowing deletion or movement.

For a read-only check of the current contexts, use `gh api repos/ScoutSportTechnology/sst-cam-firmware/rulesets` to list IDs, then `gh api repos/ScoutSportTechnology/sst-cam-firmware/rulesets/<id>` to inspect each ruleset's `required_status_checks`. A stale context from the removed `release-alpha.yml` or `release-beta.yml` workflows would block a PR and must be reported to the ruleset administrator, not bypassed here.

See [release-commit-signing.md](release-commit-signing.md) for release-tag commit provenance. The [version-reset runbook](version-reset-runbook.md) records a one-time historical procedure; it is not part of the current release flow.
