#include "app/network/services/download_server/download-server.hpp"

#include <chrono>
#include <system_error>
#include <utility>

#include <spdlog/spdlog.h>

#include "domain/common/utils/uuid.hpp"

namespace sst::network {

namespace fs = std::filesystem;

namespace {
constexpr const char* kMp4Extension = ".mp4";

auto LastWriteUnix(const fs::path& path) -> std::uint64_t {
    std::error_code ec;
    const auto ftime = fs::last_write_time(path, ec);
    if (ec) {
        return 0;
    }
    // Approximate: map file_clock to system_clock seconds since epoch.
    const auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(sys.time_since_epoch()).count());
}
}  // namespace

DownloadServer::DownloadServer(fs::path video_root, Clock clock)
    : video_root_(std::move(video_root)), clock_(std::move(clock)) {}

auto DownloadServer::Enumerate() const -> std::vector<RecordingSummary> {
    std::vector<RecordingSummary> out;
    std::error_code ec;
    if (!fs::exists(video_root_, ec)) {
        return out;
    }
    for (auto it = fs::recursive_directory_iterator(video_root_, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        const fs::path& path = it->path();
        if (!it->is_regular_file(ec) || path.extension() != kMp4Extension) {
            continue;
        }
        RecordingSummary summary;
        summary.recording_id = path.stem().string();
        summary.thumbnail_id = summary.recording_id;
        std::error_code size_ec;
        summary.size_bytes = static_cast<std::uint64_t>(fs::file_size(path, size_ec));
        summary.started_at_unix = LastWriteUnix(path);
        out.push_back(std::move(summary));
    }
    return out;
}

auto DownloadServer::ResolveRecordingPath(const std::string& recording_id) const
    -> std::optional<fs::path> {
    std::error_code ec;
    if (recording_id.empty() || !fs::exists(video_root_, ec)) {
        return std::nullopt;
    }
    for (auto it = fs::recursive_directory_iterator(video_root_, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        const fs::path& path = it->path();
        if (it->is_regular_file(ec) && path.extension() == kMp4Extension &&
            path.stem().string() == recording_id) {
            return path;
        }
    }
    return std::nullopt;
}

auto DownloadServer::MintToken(const std::string& recording_id, std::uint64_t ttl_seconds)
    -> std::optional<DownloadToken> {
    auto path = ResolveRecordingPath(recording_id);
    if (!path) {
        spdlog::warn("DownloadServer::MintToken: recording {} not found", recording_id);
        return std::nullopt;
    }
    DownloadToken token;
    token.recording_id = recording_id;
    token.token = sst::common::utils::MakeUuidV4();
    token.expires_at_unix = clock_() + ttl_seconds;
    {
        std::lock_guard lock(mtx_);
        tokens_[token.token] = Entry{.path = *path, .expires_at_unix = token.expires_at_unix};
    }
    return token;
}

auto DownloadServer::ValidateToken(const std::string& token) -> std::optional<fs::path> {
    std::lock_guard lock(mtx_);
    const auto it = tokens_.find(token);
    if (it == tokens_.end()) {
        return std::nullopt;
    }
    if (clock_() >= it->second.expires_at_unix) {
        tokens_.erase(it);
        return std::nullopt;
    }
    return it->second.path;
}

}  // namespace sst::network
