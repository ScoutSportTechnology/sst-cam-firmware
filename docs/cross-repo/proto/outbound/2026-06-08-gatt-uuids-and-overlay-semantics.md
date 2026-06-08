---
date: 2026-06-08
target_repo: proto
source_repo: firmware
status: open
kind: question
related: docs/plans/2026-06-08-001-refactor-app-source-of-truth-firmware-plan.md
---

# Placeholder GATT UUIDs + (maybe) an overlay-rendering-semantics appendix

## Context (firmware side)

The firmware refactor implements the GATT layout from `proto/README.md`:

- SST-Cam Service `A1B2C3D4-0001-0000-8000-00805F9B34FB`
- Command Write `A1B2C3D4-0011-...` (Write Without Response)
- Command Response `A1B2C3D4-0012-...` (Notify)
- Device name `sst-cam-NNNN`, service UUID advertised for the app's scan filter.

`proto/README.md` explicitly marks these UUIDs as **placeholders** ("Replace with officially registered UUIDs before production. The layout must not change without updating both this document and the firmware.").

## What firmware assumes / decided

The firmware will hard-code the placeholder UUIDs above for now (they're the agreed values today). Both the app's scan filter and the firmware's advertising use the same constants, so development works — but production needs the real registered UUIDs in **one** place (the contract repo) so neither side drifts.

## Ask for the proto repo

1. **UUID registration:** when official UUIDs are registered, update `proto/README.md` (the GATT Service Layout table) as the single source of truth, and notify both firmware and app to pick up the change in lockstep. Is there a planned date / owner for registration?
2. **Optional — overlay rendering-semantics appendix:** the overlay model in `bluetooth.proto` (coords, anchors, `text_align`, `font_size` units, wrapping, `corner_radius`, circle-from-bounds) is specified mostly in field comments. Pixel-parity between the app (Flutter) and firmware (Cairo/Pango) would be more robust if the contract carried a short, explicit "rendering semantics" section. Worth adding to the contract repo, or keep it as an app↔firmware agreement? (See the matching app-repo handoff on pixel-parity.)

## Acceptance / what "aligned" looks like

- A clear plan/owner for replacing placeholder UUIDs, with the contract repo as the authoritative source both sides track.
- A decision on whether overlay rendering semantics live in the contract repo or as an app/firmware-side agreement.

## Response (fill in from the proto session)

<left blank for the proto repo's answer/decision>
