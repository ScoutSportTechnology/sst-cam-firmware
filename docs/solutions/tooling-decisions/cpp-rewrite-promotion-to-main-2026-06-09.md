---
title: "Promoting the C++ Rewrite to main and Retiring the Python Prototype"
date: 2026-06-09
category: tooling-decisions
module: repository
problem_type: tooling_decision
component: development_workflow
severity: medium
applies_when:
  - "Replacing a prototype with a full rewrite as the source of truth while preserving history and an auditable migration path"
  - "Flipping a polyrepo's default branch to a new language implementation without rewriting history"
tags:
  - branch-promotion
  - fast-forward-merge
  - language-rewrite
  - source-of-truth
  - polyrepo
  - proto-submodule
related_components:
  - repository
  - control
  - streaming
  - overlay
---

# Promoting the C++ Rewrite to main and Retiring the Python Prototype

## Context
`main` carried the deprecated **Python prototype**; the `cpp` branch was the complete **C++/Jetson rewrite** (hexagonal architecture, proto/BLE control plane, GStreamer pipeline) and had just absorbed the PR #5 logic-alignment work. The firmware repo needed a **single source of truth** and **language consolidation** — in a polyrepo where the app and proto repos already track real contracts, the firmware default branch should reflect the shipping codebase, not the prototype. (PR #6, `cpp` → `main`, merged 2026-06-09.)

## Guidance
- `main` was **fully contained in `cpp`** — a clean fast-forward relationship, so **no history was lost**; the Python prototype stays in ancestry, recoverable from history only.
- The cutover landed as merge `459be03` ("Merge pull request #6 from ScoutSportTechnology/cpp"), parents `1d6d715` (prior `main` tip) and `1db2c57` (the PR #5 merge on `cpp`). Per project memory this required an **admin-override past branch protection** to land.
- The **proto submodule stays pinned** at `c50a216` (the amended contract the firmware now targets) — consolidation does not loosen the lockstep submodule pin that is the polyrepo's safety mechanism.

## Why This Matters
A clean cutover gives a **lossless** language migration: the default branch immediately reflects the real, shipping codebase while the prototype remains auditable in history. Every downstream consumer (CI, fresh clones, sibling repos in the polyrepo) now sees C++ as canonical, eliminating prototype/real-code drift. The earlier assumption "firmware integration branch is `cpp`, not `main`" is now retired — `cpp` content **is** `main`.

## When to Apply
- Replacing a prototype with a full rewrite where the rewrite is a superset of (or fast-forwardable over) the prototype.
- Language / source-of-truth consolidation across a polyrepo, flipping one repo's default branch to the new implementation without rewriting history.
- Not applicable when the rewrite and prototype have genuinely divergent history (no fast-forward) — that needs an explicit merge or replace strategy, not this clean promotion.

## Examples
Recovered git topology:
```
459be03  Merge pull request #6 from ScoutSportTechnology/cpp   parents: 1d6d715 1db2c57   (now: main, origin/HEAD)
1db2c57  Merge pull request #5 from ScoutSportTechnology/feat/logic-alignment
3e7038f  chore(proto): bump submodule — battery_level_pct optional (review #18)
...
proto submodule pinned: c50a216
```
PR #6: base `main` ← head `cpp`. The `cpp` ref was deleted post-merge, so the containment relationship now lives in the linear history above (`main` tip = the `cpp` content merge). The Python prototype is reachable only by walking history before the rewrite.

## Related
- Architectural origin (big-bang rewrite on feature branch): `docs/plans/2026-06-08-001-refactor-app-source-of-truth-firmware-plan.md` (`status: completed`)
- Requirements behind the refactor: `docs/brainstorms/2026-06-04-app-source-of-truth-firmware-refactor-requirements.md`
- The logic-alignment work this consolidation promoted: `docs/solutions/logic-errors/proto-contract-logic-alignment-2026-06-09.md`
- Polyrepo rationale (why submodule pins matter): project memory `monorepo-decision`.
