# Cross-Repo Coordination

This folder is the coordination lane between the **firmware** repo
(`sst-cam-firmware`, this repo) and the other repos in the SST-Cam system. The
structure is standardized across all three repos so context and handoffs stay
consistent no matter which side you're reading from.

## Layout

One folder per repo we coordinate with:

```
docs/cross-repo/<repo>/
  ├── context.md       # who that repo is, how it relates to us, standing assumptions
  ├── coordination.md  # what a change on either side forces us to do
  ├── inbound/         # handoffs/questions that came FROM that repo or a session
  └── outbound/        # handoffs we authored, sent TO that repo
```

| Folder | Repo | Role |
| --- | --- | --- |
| `app/` | `sst-cam-app` | Companion app — source of truth (users, teams, matches, overlay design, streaming config). |
| `proto/` | `sst-cam-proto` | Wire contract (`bluetooth.proto` + `wifi.proto`), pinned here as the `proto/` submodule. |

`inbound/` and `outbound/` are **optional** — create them when the first handoff
in that direction appears.

## Direction convention

Direction is **relative to this repo** (`sst-cam-firmware`):

- **`inbound/`** — it arrived from the other repo (or a session). *We* act on
  it, then fill in its `## Response` section.
- **`outbound/`** — *we* authored it for the other repo. *They* act on it, and
  their answer comes back in the `## Response` section.

Every handoff doc also carries `source_repo:` / `target_repo:` frontmatter, so
the direction is unambiguous regardless of which folder it sits in. The same
handoff appears as `outbound/` here and as `inbound/` in the repo that receives
it (e.g. our `proto/outbound/...gatt-uuids...` is `firmware/inbound/...` over in
`sst-cam-proto`).

## When to write a handoff

Create a handoff in the relevant repo's `inbound/` or `outbound/` whenever
firmware work, planning, or implementation:

1. **Assumes a workflow or behavior** owned by the other repo.
2. **Needs clarification** about how the other side behaves or what it expects.
3. **Changes the interaction** between the app and the camera firmware.
4. **Requires the other repo to build something alongside** a firmware change.

## How to use a handoff

1. The handoff is **self-contained** — written so it reads correctly in a
   *different* repo's Claude Code session with no access to this repo's context.
2. Take it into the target repo's session and run the normal
   **`/ce-brainstorm` → `/ce-plan` → `/ce-work`** process if the change is
   non-trivial, or just answer the open question.
3. Bring the answer back in the `## Response` section so the receiving side can
   act on it.

## File naming

`docs/cross-repo/<repo>/<inbound|outbound>/YYYY-MM-DD-<short-kebab-topic>.md`

## Template

```markdown
---
date: YYYY-MM-DD
source_repo: firmware | app | proto
target_repo: firmware | app | proto
status: open | in-progress | answered | done
kind: question | assumption | interaction-change | co-development
related: <plan or requirements path in the authoring repo>
---

# <Title>

## Context (authoring side)
What is happening and why this touches the other repo.

## What the author assumes / decided
The current decision or assumption, stated concretely.

## Ask for the <repo> repo
The specific question to answer, or the change/co-development to plan and build.

## Acceptance / what "aligned" looks like
How both sides know they match.

## Response (fill in from the receiving repo's session)
<left blank for the other repo's answer/decision>
```
