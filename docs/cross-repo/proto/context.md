# Context — `sst-cam-proto`

**What it is.** The single source of truth for the SST-Cam wire contract:
`bluetooth.proto` (BLE control plane — framing, `Command`/`CommandResponse`
envelopes, telemetry, match events, recording/streaming, session push, WiFi
Direct handshake, overlay layout) and `wifi.proto` (RTSP preview descriptor +
heartbeat). It carries its own `README.md` quick-reference for the wire format.

**How it relates to us.** This firmware repo (`sst-cam-firmware`) consumes
`sst-cam-proto` as a **pinned git submodule mounted at `proto/`**. Our build
codegens C++ from `proto/bluetooth.proto` + `proto/wifi.proto` (see U1) and the
firmware is, by design, a stateless executor of this contract. The companion
app (`sst-cam-app`) consumes the same contract on its side.

**Standing assumptions.**
- The schema is authoritative. When firmware behavior and the `.proto` disagree,
  the `.proto` wins — we change firmware, or we raise a handoff to change the
  contract.
- We track an exact pinned commit of the submodule. Bumping the pin is a
  deliberate act, coordinated via a handoff when it changes wire behavior.
- proto3 `optional` fields are present, so codegen needs
  `--experimental_allow_proto3_optional` on protoc 3.12.x (host) with a matching
  target `libprotobuf` (KTD1).
- The GATT UUIDs in `proto/README.md` are marked "placeholders"; firmware
  implements them as-is until proto publishes registered values
  (see `outbound/2026-06-08-gatt-uuids-and-overlay-semantics.md`).
