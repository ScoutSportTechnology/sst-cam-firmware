#pragma once

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace sst::test {

class TempDir {
   public:
    explicit TempDir(const std::string& prefix = "test") {
        const std::string tmpl =
            (std::filesystem::temp_directory_path() / ("sst-" + prefix + "-XXXXXX")).string();
        std::vector<char> buffer(tmpl.begin(), tmpl.end());
        buffer.push_back('\0');
        if (mkdtemp(buffer.data()) == nullptr) {
            throw std::system_error(errno, std::generic_category(),
                                    "mkdtemp failed for test temp directory");
        }
        path_ = buffer.data();
    }
    ~TempDir() {
        std::error_code err;
        std::filesystem::remove_all(path_, err);
    }
    TempDir(const TempDir&) = delete;
    auto operator=(const TempDir&) -> TempDir& = delete;
    TempDir(TempDir&&) = delete;
    auto operator=(TempDir&&) -> TempDir& = delete;

    [[nodiscard]] auto path() const -> const std::filesystem::path& { return path_; }

   private:
    std::filesystem::path path_;
};

}  // namespace sst::test