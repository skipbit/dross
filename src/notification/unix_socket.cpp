#include "notification/unix_socket.h"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace dross {

namespace {

error last_error(const char* call)
{
    return error{ static_cast<std::errc>(errno), call };
}

std::expected<sockaddr_un, error> address_of(const std::string& name)
{
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (name.empty() || (name.size() >= sizeof(address.sun_path))) {
        return std::unexpected(error{ std::errc::filename_too_long, name });
    }
    std::memcpy(address.sun_path, name.data(), name.size());
    return address;
}

const sockaddr* as_socket_address(const sockaddr_un& address)
{
    return reinterpret_cast<const sockaddr*>(&address);
}

std::expected<socket_handle, error> connect_to(const sockaddr_un& address)
{
    auto made = open_unix_socket();
    if (! made) {
        return made;
    }
    if (::connect(made->get(), as_socket_address(address), sizeof(address)) != 0) {
        return std::unexpected(last_error("connect"));
    }
    return made;
}

// Removes a socket file that refuses connections, which a process that has
// gone leaves behind.
std::expected<void, error> remove_stale(const std::string& name, const sockaddr_un& address)
{
    struct stat found { };
    if (::lstat(name.c_str(), &found) != 0) {
        if (errno == ENOENT) {
            return {};
        }
        return std::unexpected(last_error("lstat"));
    }
    if (! S_ISSOCK(found.st_mode)) {
        return std::unexpected(error{ std::errc::address_in_use, name });
    }
    auto probe = connect_to(address);
    if (probe) {
        return std::unexpected(error{ std::errc::address_in_use, name });
    }
    if (probe.error() != error(std::errc::connection_refused)) {
        return std::unexpected(std::move(probe).error());
    }
    if ((::unlink(name.c_str()) != 0) && (errno != ENOENT)) {
        return std::unexpected(last_error("unlink"));
    }
    return {};
}

}  // namespace

socket_handle::socket_handle(const int fd) noexcept
    : _fd{ fd }
{
}

socket_handle::socket_handle(socket_handle&& other) noexcept
    : _fd{ std::exchange(other._fd, -1) }
{
}

socket_handle& socket_handle::operator=(socket_handle&& other) noexcept
{
    if (this != &other) {
        if (_fd >= 0) {
            ::close(_fd);
        }
        _fd = std::exchange(other._fd, -1);
    }
    return *this;
}

socket_handle::~socket_handle()
{
    if (_fd >= 0) {
        ::close(_fd);
    }
}

int socket_handle::get() const noexcept
{
    return _fd;
}

std::expected<unix_listener, error> unix_listener::listen(const path& location)
{
    std::string name = location.string();
    const auto address = address_of(name);
    if (! address) {
        return std::unexpected(address.error());
    }
    auto socket = open_unix_socket();
    if (! socket) {
        return std::unexpected(std::move(socket).error());
    }
    if (::bind(socket->get(), as_socket_address(*address), sizeof(*address)) != 0) {
        if (errno != EADDRINUSE) {
            return std::unexpected(last_error("bind"));
        }
        if (auto removed = remove_stale(name, *address); ! removed) {
            return std::unexpected(std::move(removed).error());
        }
        if (::bind(socket->get(), as_socket_address(*address), sizeof(*address)) != 0) {
            return std::unexpected(last_error("bind"));
        }
    }
    struct stat bound { };
    if (::lstat(name.c_str(), &bound) != 0) {
        return std::unexpected(last_error("lstat"));
    }
    // From here the destructor removes the socket file on failure.
    unix_listener made{ std::move(*socket), std::move(name), bound.st_dev, bound.st_ino };
    if (::chmod(made._name.c_str(), S_IRUSR | S_IWUSR) != 0) {
        return std::unexpected(last_error("chmod"));
    }
    if (::listen(made._socket.get(), SOMAXCONN) != 0) {
        return std::unexpected(last_error("listen"));
    }
    return made;
}

unix_listener::unix_listener(socket_handle socket, std::string name, const dev_t device, const ino_t inode) noexcept
    : _socket{ std::move(socket) }
    , _name{ std::move(name) }
    , _device{ device }
    , _inode{ inode }
{
}

unix_listener::~unix_listener()
{
    if (_socket.get() < 0) {
        return;
    }
    struct stat found { };
    if ((::lstat(_name.c_str(), &found) == 0) && (found.st_dev == _device) && (found.st_ino == _inode)) {
        (void)::unlink(_name.c_str());
    }
}

int unix_listener::fd() const noexcept
{
    return _socket.get();
}

std::expected<std::optional<socket_handle>, error> unix_listener::accept()
{
    return accept_unix_socket(_socket.get());
}

std::expected<std::optional<std::size_t>, error> receive_some(const int fd, const std::span<std::byte> into)
{
    const ssize_t received = ::recv(fd, into.data(), into.size(), 0);
    if (received < 0) {
        if ((errno == EAGAIN) || (errno == EINTR)) {
            return std::nullopt;
        }
        return std::unexpected(last_error("recv"));
    }
    return static_cast<std::size_t>(received);
}

std::expected<socket_handle, error> unix_connect(const path& location)
{
    const auto address = address_of(location.string());
    if (! address) {
        return std::unexpected(address.error());
    }
    return connect_to(*address);
}

}  // namespace dross
