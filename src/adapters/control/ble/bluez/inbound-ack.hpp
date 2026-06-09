#pragma once

#include <cstdint>
#include <string>

#include "bluetooth.pb.h"

namespace sst::adapters::control {

// Build the ChunkAck the firmware emits for one inbound (app->camera) command
// chunk. Pure (no D-Bus) so the per-chunk ack policy is unit-testable
// transport-free; the BlueZ transport serializes the result and notifies it on
// the response characteristic. ChunkAck is wire-compatible with ChunkedPayload
// on fields 1/2 and carries no total_chunks, so the app demuxes it as an ack
// (total_chunks == 0) rather than a response chunk.
inline auto BuildInboundAck(const std::string& correlation_id, std::uint32_t chunk_index)
    -> sst_cam::ChunkAck {
    sst_cam::ChunkAck ack;
    ack.set_correlation_id(correlation_id);
    ack.set_chunk_index(chunk_index);
    return ack;
}

}  // namespace sst::adapters::control
