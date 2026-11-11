#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "../support/temp-dir.hpp"
#include "app/control/services/handlers/download.handler.hpp"
#include "app/network/services/download_server/download-server.hpp"
#include "bluetooth.pb.h"

namespace fs = std::filesystem;

namespace {

using sst::control::DownloadHandler;
using sst::network::DownloadServer;
using sst::test::TempDir;

auto WriteFile(const fs::path& path, const std::string& body) -> void {
    std::ofstream out(path, std::ios::binary);
    out << body;
}

auto ListCmd() -> sst_cam::Command {
    sst_cam::Command cmd;
    cmd.set_correlation_id("l");
    cmd.mutable_list_recordings();
    return cmd;
}

TEST(DownloadHandlerTest, ProxyIsNotListedOrTokenedByGuessedId) {
    TempDir temp_dir("download");
    const fs::path& root = temp_dir.path();
    WriteFile(root / "match-x.mp4", "final");
    WriteFile(root / "raw__match-x__cam0.mp4", "private");
    constexpr std::uint32_t kDownloadPort = 8080;
    constexpr std::uint64_t kTokenTtlSeconds = 3600;
    DownloadServer server(root, root, [] { return std::uint64_t{0}; });
    DownloadHandler handler(server, "192.168.49.1", kDownloadPort, kTokenTtlSeconds);
    const auto listed = handler.Handle(ListCmd());
    ASSERT_EQ(listed.recording_list().recordings_size(), 1);
    EXPECT_EQ(listed.recording_list().recordings(0).id(), "match-x");
    sst_cam::Command request;
    request.mutable_download_request()->set_recording_id("raw__match-x__cam0");
    EXPECT_EQ(handler.Handle(request).status(), sst_cam::ERROR);
    request.mutable_download_request()->set_recording_id("match-x");
    EXPECT_EQ(handler.Handle(request).status(), sst_cam::OK);
}

TEST(DownloadHandlerTest, ListReportsRecordingMetadata) {
    TempDir temp_dir("download");
    const fs::path& root = temp_dir.path();
    WriteFile(root / "match-x.mp4", "final");

    constexpr std::uint32_t kDownloadPort = 8080;
    constexpr std::uint64_t kTokenTtlSeconds = 3600;
    DownloadServer server(root, root, [] { return std::uint64_t{0}; });
    DownloadHandler handler(server, "192.168.49.1", kDownloadPort,
                            /*token_ttl_seconds=*/kTokenTtlSeconds);

    auto resp = handler.Handle(ListCmd());
    ASSERT_EQ(resp.status(), sst_cam::ResponseStatus::OK);
    ASSERT_EQ(resp.payload_case(), sst_cam::CommandResponse::kRecordingList);
    ASSERT_EQ(resp.recording_list().recordings_size(), 1);
    EXPECT_EQ(resp.recording_list().recordings(0).id(), "match-x");
    EXPECT_EQ(resp.recording_list().recordings(0).size_bytes(), 5U);
}

}  // namespace
