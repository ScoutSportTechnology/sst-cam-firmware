// ChunkAssembler framing + ChunkAck flow control (U4, R1/R3).
//
// Pure — no D-Bus. The assembler is the transport-free core of the BLE framing;
// the BlueZ wiring around it is exercised by a hardware-bound test.

#include "adapters/control/ble/bluez/chunk-assembler.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "bluetooth.pb.h"

namespace {

using sst::adapters::control::ChunkAssembler;
using sst::adapters::control::ChunkAssemblerConfig;

auto MakeChunk(const std::string& corr, std::uint32_t index, std::uint32_t total,
               const std::string& data) -> sst_cam::ChunkedPayload {
    sst_cam::ChunkedPayload c;
    c.set_correlation_id(corr);
    c.set_chunk_index(index);
    c.set_total_chunks(total);
    c.set_data(data);
    return c;
}

// Serialize a Command and split it into N chunks of `chunk_size` bytes.
auto SplitCommand(const sst_cam::Command& cmd, const std::string& corr, std::size_t chunk_size)
    -> std::vector<sst_cam::ChunkedPayload> {
    const std::string wire = cmd.SerializeAsString();
    std::vector<sst_cam::ChunkedPayload> out;
    const std::uint32_t total =
        static_cast<std::uint32_t>((wire.size() + chunk_size - 1) / chunk_size);
    for (std::uint32_t i = 0; i < total; ++i) {
        out.push_back(MakeChunk(corr, i, total, wire.substr(i * chunk_size, chunk_size)));
    }
    return out;
}

// R3: a multi-chunk PushOverlayLayout reassembles to a byte-identical Command,
// and no decode happens before the final chunk arrives.
TEST(ChunkAssemblerTest, MultiChunkInboundReassemblesExactly) {
    sst_cam::Command cmd;
    cmd.set_correlation_id("layout-1");
    auto* layout = cmd.mutable_push_overlay_layout()->mutable_layout();
    layout->set_canvas_width(1920);
    layout->set_canvas_height(1080);
    for (int i = 0; i < 40; ++i) {
        auto* el = layout->add_elements();
        el->set_id("element-with-a-longish-id-" + std::to_string(i));
    }
    const std::string wire = cmd.SerializeAsString();

    ChunkAssembler assembler;
    auto chunks = SplitCommand(cmd, "layout-1", 16);
    ASSERT_GT(chunks.size(), 1U);

    std::optional<std::string> done;
    for (std::size_t i = 0; i + 1 < chunks.size(); ++i) {
        EXPECT_FALSE(assembler.OfferInbound(chunks[i]).has_value())
            << "decoded before the final chunk at index " << i;
    }
    done = assembler.OfferInbound(chunks.back());
    ASSERT_TRUE(done.has_value());
    EXPECT_EQ(*done, wire);

    sst_cam::Command parsed;
    ASSERT_TRUE(parsed.ParseFromString(*done));
    EXPECT_EQ(parsed.push_overlay_layout().layout().elements_size(), 40);
    EXPECT_EQ(assembler.InflightInboundCount(), 0U);  // state freed on completion
}

// Single-chunk message decodes immediately, no reassembly buffer retained.
TEST(ChunkAssemblerTest, SingleChunkFastPath) {
    ChunkAssembler assembler;
    auto chunk = MakeChunk("c1", 0, 1, "hello");
    auto done = assembler.OfferInbound(chunk);
    ASSERT_TRUE(done.has_value());
    EXPECT_EQ(*done, "hello");
    EXPECT_EQ(assembler.InflightInboundCount(), 0U);
}

// R3: outbound emits chunk 0, blocks until its ack, then emits chunk 1, etc.
TEST(ChunkAssemblerTest, OutboundIsGatedByChunkAck) {
    ChunkAssemblerConfig cfg;
    cfg.max_chunk_payload_bytes = 4;
    ChunkAssembler assembler(cfg);

    std::vector<std::uint32_t> sent;
    auto send = [&](const sst_cam::ChunkedPayload& c) { sent.push_back(c.chunk_index()); };

    const std::string data = "abcdefghij";  // 10 bytes -> 3 chunks of 4
    const std::uint32_t total = assembler.BeginOutbound("r1", data, send);
    ASSERT_EQ(total, 3U);

    // Only chunk 0 is out so far.
    EXPECT_EQ(sent, (std::vector<std::uint32_t>{0}));

    // Acking the wrong (not-yet-sent) index must not release anything.
    EXPECT_FALSE(assembler.OnAck("r1", 1));
    EXPECT_EQ(sent, (std::vector<std::uint32_t>{0}));

    EXPECT_FALSE(assembler.OnAck("r1", 0));
    EXPECT_EQ(sent, (std::vector<std::uint32_t>{0, 1}));

    EXPECT_FALSE(assembler.OnAck("r1", 1));
    EXPECT_EQ(sent, (std::vector<std::uint32_t>{0, 1, 2}));

    // Acking the final chunk completes the transfer and frees state.
    EXPECT_TRUE(assembler.OnAck("r1", 2));
    EXPECT_EQ(assembler.PendingOutboundCount(), 0U);
}

// Single-chunk outbound is fire-and-forget: chunk 0 sent, no retained state.
TEST(ChunkAssemblerTest, SingleChunkOutboundNeedsNoAck) {
    ChunkAssembler assembler;
    std::vector<std::uint32_t> sent;
    auto send = [&](const sst_cam::ChunkedPayload& c) { sent.push_back(c.chunk_index()); };

    EXPECT_EQ(assembler.BeginOutbound("r2", "tiny", send), 1U);
    EXPECT_EQ(sent.size(), 1U);
    EXPECT_EQ(assembler.PendingOutboundCount(), 0U);
    // A stray ack for an unknown transfer is harmless.
    EXPECT_FALSE(assembler.OnAck("r2", 0));
}

// Robustness: out-of-order arrival + a duplicate chunk still reassemble once.
TEST(ChunkAssemblerTest, OutOfOrderAndDuplicateInbound) {
    ChunkAssembler assembler;
    EXPECT_FALSE(assembler.OfferInbound(MakeChunk("x", 2, 3, "ccc")).has_value());
    EXPECT_FALSE(assembler.OfferInbound(MakeChunk("x", 0, 3, "aaa")).has_value());
    EXPECT_FALSE(assembler.OfferInbound(MakeChunk("x", 0, 3, "aaa")).has_value());  // dup
    auto done = assembler.OfferInbound(MakeChunk("x", 1, 3, "bbb"));
    ASSERT_TRUE(done.has_value());
    EXPECT_EQ(*done, "aaabbbccc");
}

// Malformed framing is rejected without crashing or retaining state.
TEST(ChunkAssemblerTest, MalformedChunksRejected) {
    ChunkAssembler assembler;
    EXPECT_FALSE(assembler.OfferInbound(MakeChunk("m", 0, 0, "x")).has_value());   // total 0
    EXPECT_FALSE(assembler.OfferInbound(MakeChunk("m", 5, 3, "x")).has_value());   // index>=total
    EXPECT_EQ(assembler.InflightInboundCount(), 0U);
}

// Never-completing reassemblies are evicted past the in-flight cap — no leak.
TEST(ChunkAssemblerTest, InflightCapEvictsStalePartials) {
    ChunkAssemblerConfig cfg;
    cfg.max_inflight_inbound = 4;
    ChunkAssembler assembler(cfg);

    for (int i = 0; i < 100; ++i) {
        // Each is the first of two chunks and never completes.
        assembler.OfferInbound(MakeChunk("corr-" + std::to_string(i), 0, 2, "partial"));
    }
    EXPECT_LE(assembler.InflightInboundCount(), cfg.max_inflight_inbound);
}

}  // namespace
