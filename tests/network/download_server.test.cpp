// Download server: tokens, enumeration, and byte-range HTTP over loopback
// (U13, R24, F4).
//
// The token-mint/validate/expiry and enumeration tests are the in-container
// module-boundary coverage. The end-to-end HTTP test stands up a real
// cpp-httplib server + client on 127.0.0.1; it passes natively / on-device but
// is ENVIRONMENT-BOUND — qemu-user (the cross-compile container's aarch64
// emulator) does not run the threaded accept loop, so it fails here, like the
// other on-device e2e tests. It is committed and run for real on the device.

#include "app/network/services/download_server/download-server.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "adapters/network/http/http-download-server.hpp"
#include "httplib.h"

namespace fs = std::filesystem;

namespace {

using sst::network::DownloadServer;

auto MakeRoot() -> fs::path {
    static std::atomic<int> n{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path root = fs::temp_directory_path() /
                    ("sst_dl_" + std::to_string(stamp) + "_" + std::to_string(n.fetch_add(1)));
    fs::create_directories(root);
    return root;
}

auto WriteRecording(const fs::path& root, const std::string& match, const std::string& body)
    -> void {
    const fs::path dir = root / "user" / match;
    fs::create_directories(dir);
    std::ofstream out(dir / (match + ".mp4"), std::ios::binary);
    out << body;
}

// Enumeration finds MP4s on disk with their sizes.
TEST(DownloadServerTest, EnumeratesRecordings) {
    const fs::path root = MakeRoot();
    WriteRecording(root, "match-a", "aaaa");
    WriteRecording(root, "match-b", "bbbbbb");
    DownloadServer server(root, [] { return std::uint64_t{100}; });

    auto recs = server.Enumerate();
    ASSERT_EQ(recs.size(), 2U);
    std::uint64_t total = 0;
    for (const auto& r : recs) {
        total += r.size_bytes;
        EXPECT_EQ(r.thumbnail_id, r.recording_id);
    }
    EXPECT_EQ(total, 10U);
    fs::remove_all(root);
}

// A token validates until it expires; an unknown token never validates.
TEST(DownloadServerTest, TokenMintValidateExpire) {
    const fs::path root = MakeRoot();
    WriteRecording(root, "match-a", "data");
    std::uint64_t now = 1000;
    DownloadServer server(root, [&now] { return now; });

    auto token = server.MintToken("match-a", /*ttl=*/60);
    ASSERT_TRUE(token.has_value());
    EXPECT_EQ(token->expires_at_unix, 1060U);
    EXPECT_TRUE(server.ValidateToken(token->token).has_value());

    EXPECT_FALSE(server.ValidateToken("bogus-token").has_value());

    now = 1060;  // at expiry
    EXPECT_FALSE(server.ValidateToken(token->token).has_value());

    // A token for a non-existent recording is not minted.
    EXPECT_FALSE(server.MintToken("ghost", 60).has_value());
    fs::remove_all(root);
}

// End-to-end over loopback: a minted token authorizes a full + ranged GET;
// missing/foreign tokens are rejected with 401. ENVIRONMENT-BOUND: passes
// natively / on-device, fails under the container's qemu-user (see file header).
TEST(DownloadServerTest, HttpServesByteRangeWithBearerToken) {
    const fs::path root = MakeRoot();
    const std::string body = "0123456789ABCDEF";
    WriteRecording(root, "match-a", body);
    DownloadServer downloads(root, [] {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    });
    auto token = downloads.MintToken("match-a", 3600);
    ASSERT_TRUE(token.has_value());

    sst::adapters::network::HttpDownloadServer http(
        "127.0.0.1", 0,
        [&downloads](const std::string& t) { return downloads.ValidateToken(t); });
    ASSERT_TRUE(http.Start());
    const std::uint16_t port = http.BoundPort();
    ASSERT_GT(port, 0);

    httplib::Client client("127.0.0.1", port);
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(2, 0);

    // Wait for the listener to accept connections (qemu scheduling can lag).
    httplib::Result probe;
    for (int i = 0; i < 100; ++i) {
        probe = client.Get("/recordings/match-a");
        if (probe) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // No token -> 401.
    {
        ASSERT_TRUE(probe) << "could not connect to loopback HTTP server";
        EXPECT_EQ(probe->status, 401);
    }
    // Foreign token -> 401.
    {
        auto res = client.Get("/recordings/match-a", {{"Authorization", "Bearer nope"}});
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status, 401);
    }
    // Valid token -> 200 + full body.
    {
        auto res = client.Get("/recordings/match-a",
                              {{"Authorization", "Bearer " + token->token}});
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status, 200);
        EXPECT_EQ(res->body, body);
    }
    // Valid token + Range -> 206 + slice.
    {
        auto res = client.Get("/recordings/match-a",
                              {{"Authorization", "Bearer " + token->token},
                               {"Range", "bytes=4-7"}});
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status, 206);
        EXPECT_EQ(res->body, "4567");
    }

    http.Stop();
    fs::remove_all(root);
}

}  // namespace
