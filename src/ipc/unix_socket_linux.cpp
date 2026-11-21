#include "ipc/last_error.h"
#include "ipc/unix_socket.h"

#include <cerrno>
#include <sys/socket.h>

namespace dross {

std::expected<socket_handle, error> open_unix_socket()
{
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return std::unexpected(last_error("socket"));
    }
    return socket_handle{ fd };
}

std::expected<std::optional<socket_handle>, error> accept_unix_socket(const int listening)
{
    const int fd = ::accept4(listening, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd < 0) {
        if ((errno == EAGAIN) || (errno == EINTR) || (errno == ECONNABORTED)) {
            return std::nullopt;
        }
        return std::unexpected(last_error("accept4"));
    }
    return socket_handle{ fd };
}

std::expected<uid_t, error> peer_uid(const int fd)
{
    ucred credentials{};
    socklen_t length = sizeof(credentials);
    if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &length) != 0) {
        return std::unexpected(last_error("getsockopt"));
    }
    return credentials.uid;
}

// MSG_NOSIGNAL keeps a peer that has gone from raising SIGPIPE.
std::expected<std::size_t, error> send_some(const int fd, const std::span<const std::byte> bytes)
{
    const ssize_t sent = ::send(fd, bytes.data(), bytes.size(), MSG_NOSIGNAL);
    if (sent < 0) {
        if ((errno == EAGAIN) || (errno == EINTR)) {
            return 0;
        }
        return std::unexpected(last_error("send"));
    }
    return static_cast<std::size_t>(sent);
}

}  // namespace dross
