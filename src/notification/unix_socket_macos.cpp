#include "notification/unix_socket.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace dross {

namespace {

error last_error(const char* call)
{
    return error{ static_cast<std::errc>(errno), call };
}

// macOS has neither the socket() flags nor accept4(), so each socket is set
// up after it is made. SO_NOSIGPIPE keeps a peer that has gone from raising
// SIGPIPE.
std::expected<socket_handle, error> set_up(socket_handle socket)
{
    const int fd = socket.get();
    if (::fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {
        return std::unexpected(last_error("fcntl"));
    }
    const int status = ::fcntl(fd, F_GETFL);
    if ((status < 0) || (::fcntl(fd, F_SETFL, status | O_NONBLOCK) != 0)) {
        return std::unexpected(last_error("fcntl"));
    }
    const int on = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) != 0) {
        return std::unexpected(last_error("setsockopt"));
    }
    return socket;
}

}  // namespace

std::expected<socket_handle, error> open_unix_socket()
{
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return std::unexpected(last_error("socket"));
    }
    return set_up(socket_handle{ fd });
}

std::expected<std::optional<socket_handle>, error> accept_unix_socket(const int listening)
{
    const int fd = ::accept(listening, nullptr, nullptr);
    if (fd < 0) {
        if ((errno == EAGAIN) || (errno == EINTR) || (errno == ECONNABORTED)) {
            return std::nullopt;
        }
        return std::unexpected(last_error("accept"));
    }
    auto made = set_up(socket_handle{ fd });
    if (! made) {
        return std::unexpected(std::move(made).error());
    }
    return std::move(*made);
}

std::expected<uid_t, error> peer_uid(const int fd)
{
    uid_t user = 0;
    gid_t group = 0;
    if (::getpeereid(fd, &user, &group) != 0) {
        return std::unexpected(last_error("getpeereid"));
    }
    return user;
}

std::expected<std::size_t, error> send_some(const int fd, const std::span<const std::byte> bytes)
{
    const ssize_t sent = ::send(fd, bytes.data(), bytes.size(), 0);
    if (sent < 0) {
        if ((errno == EAGAIN) || (errno == EINTR)) {
            return 0;
        }
        return std::unexpected(last_error("send"));
    }
    return static_cast<std::size_t>(sent);
}

}  // namespace dross
