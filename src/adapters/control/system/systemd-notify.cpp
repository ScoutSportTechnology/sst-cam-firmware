#include "adapters/control/system/systemd-notify.hpp"

#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace sst::adapters::control {

auto NotifySystemd(const char* message) -> bool {
    const char* path = std::getenv("NOTIFY_SOCKET");
    if (path == nullptr || *path == '\0') {
        return true;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    const std::size_t length = std::strlen(path);
    if (length >= sizeof(addr.sun_path)) {
        spdlog::warn("sd_notify: socket path too long");
        return false;
    }
    std::memcpy(addr.sun_path, path, length + 1);
    if (path[0] == '@') {
        addr.sun_path[0] = '\0';
    }
    const int socket_fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (socket_fd < 0) {
        spdlog::warn("sd_notify: socket failed: {}", std::strerror(errno));
        return false;
    }
    const ssize_t sent =
        ::sendto(socket_fd, message, std::strlen(message), MSG_NOSIGNAL,
                 reinterpret_cast<const sockaddr*>(&addr),
                 static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + length + 1));
    if (sent < 0) {
        spdlog::warn("sd_notify: sendto failed: {}", std::strerror(errno));
    }
    ::close(socket_fd);
    return sent == static_cast<ssize_t>(std::strlen(message));
}

}  // namespace sst::adapters::control
