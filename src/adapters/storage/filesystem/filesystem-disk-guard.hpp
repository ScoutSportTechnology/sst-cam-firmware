#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include "app/storage/ports/disk-guard.hpp"

namespace sst::adapters::storage {

// std::filesystem::space-based disk guard. Walks up from `root` to find an
// existing parent if the configured directory hasn't been created yet — that
// way the first-ever recording on a fresh device still gets a sensible
// answer (rather than failing on stat of a non-existent path).
class FilesystemDiskGuard final : public sst::storage::IDiskGuard {
   public:
    FilesystemDiskGuard(std::filesystem::path root, std::optional<std::uint64_t> min_free_bytes);

    [[nodiscard]] auto HasEnoughFreeSpace() const -> bool override;
    [[nodiscard]] auto FreeBytes() const -> std::uint64_t override;

   private:
    [[nodiscard]] auto ResolveExistingPath() const -> std::filesystem::path;

    std::filesystem::path root_;
    std::optional<std::uint64_t> min_free_bytes_;
};

}  // namespace sst::adapters::storage
