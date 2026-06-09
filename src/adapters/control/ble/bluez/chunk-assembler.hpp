#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "bluetooth.pb.h"

namespace sst::adapters::control {

struct ChunkAssemblerConfig {
    // Bytes of `data` per outbound chunk. Sized under the negotiated MTU (512
    // requested after connect) minus ChunkedPayload + ATT framing overhead.
    std::size_t max_chunk_payload_bytes = 400;
    // Cap on concurrent inbound reassemblies (correlation_ids). A flood of
    // never-completing partial messages can't grow memory without bound — the
    // oldest incomplete reassembly is evicted past this cap.
    std::size_t max_inflight_inbound = 16;
    // Reject absurd total_chunks up front (malformed / hostile).
    std::uint32_t max_total_chunks = 8192;
};

// Pure (no D-Bus) ChunkedPayload reassembly + ChunkAck-gated outbound chunking.
// The BlueZ transport feeds inbound chunks in and drives outbound chunks out
// through this; keeping it transport-free makes the framing logic unit-testable.
//
// Inbound:  OfferInbound() accumulates chunks keyed by correlation_id and
//           returns the reassembled inner payload (a serialized Command) once
//           the final chunk arrives. Single-chunk messages return immediately.
// Outbound: BeginOutbound() splits a serialized CommandResponse and emits chunk
//           0; each ChunkAck (OnAck) releases the next chunk — never sending a
//           chunk ahead of its predecessor's ack (R3 flow control).
class ChunkAssembler {
   public:
    using SendChunkFn = std::function<void(const sst_cam::ChunkedPayload&)>;

    explicit ChunkAssembler(ChunkAssemblerConfig cfg = {});

    // Returns the fully-reassembled inner payload bytes when `chunk` completes
    // its message; std::nullopt while more chunks are pending or on a
    // malformed / duplicate chunk.
    auto OfferInbound(const sst_cam::ChunkedPayload& chunk) -> std::optional<std::string>;

    // Split `data` for `correlation_id` and emit chunk 0 via `send`. Multi-chunk
    // transfers retain state until acked; a single-chunk transfer is fire-and-
    // forget. Returns the total chunk count.
    auto BeginOutbound(const std::string& correlation_id, const std::string& data,
                       const SendChunkFn& send) -> std::uint32_t;

    // Process a ChunkAck. If it acks the most-recently-sent chunk, the next
    // chunk is emitted via the transfer's stored send fn. Returns true once the
    // whole message has been delivered and acked (transfer complete).
    auto OnAck(const std::string& correlation_id, std::uint32_t chunk_index) -> bool;

    [[nodiscard]] auto InflightInboundCount() const -> std::size_t { return inbound_.size(); }
    [[nodiscard]] auto PendingOutboundCount() const -> std::size_t { return outbound_.size(); }

   private:
    struct InboundState {
        std::uint32_t total_chunks{0};
        std::uint32_t received{0};
        std::vector<std::string> parts;
        std::vector<bool> have;
    };

    struct OutboundState {
        std::vector<sst_cam::ChunkedPayload> chunks;
        std::uint32_t next_to_send{0};  // index of the next not-yet-sent chunk
        SendChunkFn send;
    };

    auto EvictOldestInboundIfFull() -> void;

    ChunkAssemblerConfig cfg_;
    std::unordered_map<std::string, InboundState> inbound_;
    std::list<std::string> inbound_order_;  // insertion order, for eviction
    std::unordered_map<std::string, OutboundState> outbound_;
};

}  // namespace sst::adapters::control
