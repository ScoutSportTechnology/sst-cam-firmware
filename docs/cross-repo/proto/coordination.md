# Coordination — `sst-cam-proto`

What a change on either side forces the other to do.

## When proto changes (proto → firmware)
- **Field added/removed/renumbered, message added, enum value added.** Re-pin the
  `proto/` submodule to the new commit, re-run codegen (it is automatic on
  `cmake --preset`), and wire any new `Command` oneof case into the dispatcher
  (a new `ICommandHandler`, U3/U14). A removed/renumbered field is a breaking
  change — expect a coordinated firmware + app release and a
  `DeviceInfoResponse.protocol_version` bump.
- **`ResponseStatus` / framing (`ChunkedPayload`, `ChunkAck`) changes.** Touches
  the dispatcher (U3) and the BLE framing (U4) directly — treat as breaking.
- **Overlay schema (`OverlayLayout`/`OverlayElement`/bindings/templates).**
  Touches the overlay domain mapping + renderer (U8) and the scene/event wiring
  (U9).

## When firmware needs a contract change (firmware → proto)
- Raise an `outbound/` handoff describing the wire change firmware needs (new
  command, new field, clarified semantics). Do **not** edit generated code or
  fork the schema locally — change flows through the proto repo, then we re-pin.

## Compatibility rules (from `proto/README.md`)
- Adding `optional` fields is backward-compatible.
- Tombstone removed field numbers with `reserved`; never reuse them.
- Bump `DeviceInfoResponse.protocol_version` on breaking changes; the app warns
  and disables unsupported features when firmware is behind.

## Build coupling
- Host `protoc` and target `libprotobuf` must be the **same** upstream protobuf
  version (jammy 3.12.4, KTD1). A proto change that requires a newer protobuf is
  a sysroot + Dockerfile change here, not just a re-pin.
