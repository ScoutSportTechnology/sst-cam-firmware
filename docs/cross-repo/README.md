# Cross-Repo Handoffs

This folder is the coordination lane between the **firmware** repo (`sst-cam-firmware`, this repo) and the other repos in the SST-Cam system. Each connected repo has its own subfolder:

| Folder | Repo | Role |
| --- | --- | --- |
| `app/` | `sst-cam-app` | Companion app — source of truth (users, teams, matches, overlay design, streaming config). |
| `proto/` | `sst-cam-proto` | Wire contract (`bluetooth.proto` + `wifi.proto`), pinned here as the `proto/` submodule. |

## When to write a handoff

Create a markdown file in the relevant repo's subfolder whenever firmware work, planning, or implementation:

1. **Assumes a workflow or behavior** owned by the other repo (e.g. "the app sends `StartWifiDirect` right after `GetDeviceInfo`").
2. **Needs clarification** about how the other side behaves or what it expects.
3. **Changes the interaction** between the app and the camera firmware (a command's semantics, a credential format, an overlay-rendering agreement, a GATT detail).
4. **Requires the other repo to build something alongside** a firmware change (a new command, a new field, a new flow that both sides must implement).

The same rule applies in reverse: the other repos write handoffs back into their own copy of this lane (or drop a response into the `## Response` section below).

## How to use a handoff

1. The handoff is **self-contained** — written so it can be read in a *different* repo's Claude Code session with no access to this repo's context.
2. Take the file into the target repo's session and run the normal **`/ce-brainstorm` → `/ce-plan` → `/ce-work`** process against it if the change is non-trivial, or just ask that repo's Claude to answer the open question.
3. Bring the answer/decision back: fill in the `## Response` section (or paste the reply here) so the firmware side can act on it.

## File naming

`docs/cross-repo/<repo>/YYYY-MM-DD-<short-kebab-topic>.md`

## Template

```markdown
---
date: YYYY-MM-DD
target_repo: app | proto
source_repo: firmware
status: open | answered | in-progress | done
kind: question | assumption | interaction-change | co-development
related: <plan or requirements path in this repo>
---

# <Title>

## Context (firmware side)
What is happening here and why this touches the other repo.

## What firmware assumes / decided
The current firmware-side decision or assumption, stated concretely.

## Ask for the <repo> repo
The specific question to answer, or the change/co-development to plan and build.

## Acceptance / what "aligned" looks like
How both sides know they match.

## Response (fill in from the <repo> session)
<left blank for the other repo's answer/decision>
```
