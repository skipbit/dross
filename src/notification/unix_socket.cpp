#include "notification/unix_socket.h"

#include "notification/last_error.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace dross {

namespace {

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
    // Another process may have replaced the file since it was probed.
    struct stat probed { };
    if ((::lstat(name.c_str(), &probed) != 0) || (probed.st_dev != found.st_dev) || (probed.st_ino != found.st_ino)) {
        return {};
    }
    if ((::unlink(name.c_str()) != 0) && (errno != ENOENT)) {
        return std::unexpected(last_error("unlink"));
    }
    return {};
}

// A name next to name, of the same length on every call. The number after
// the process id differs between the calls of one process, and depends on
// the time, so other processes with the same id rarely make it too.
std::string temporary_name_for(const std::string& name)
{
    static std::atomic<std::uint32_t> made{ 0 };
    const auto now = static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uint32_t number = now ^ (made++ * 0x9e3779b9U);
    std::string digits(8, '0');
    for (auto each = digits.rbegin(); each != digits.rend(); ++each) {
        *each = "0123456789abcdef"[number % 16];
        number /= 16;
    }
    return name + "." + std::to_string(::getpid()) + "." + digits;
}

// How many names listen() tries before it gives up.
constexpr int temporary_name_tries = 4;

// Puts the socket file at from also at name. link() fails rather than
// replace a file at name, so a file someone listens on is never replaced.
std::expected<void, error> place(const std::string& from, const std::string& name, const sockaddr_un& address)
{
    if (::link(from.c_str(), name.c_str()) == 0) {
        return {};
    }
    if (errno != EEXIST) {
        return std::unexpected(last_error("link"));
    }
    if (auto removed = remove_stale(name, address); ! removed) {
        return removed;
    }
    if (::link(from.c_str(), name.c_str()) == 0) {
        return {};
    }
    if (errno == EEXIST) {
        return std::unexpected(error{ std::errc::address_in_use, name });
    }
    return std::unexpected(last_error("link"));
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
    // The socket listens under a name of its own before it appears at name,
    // so no one finds it there refusing connections and takes it for stale.
    auto socket = open_unix_socket();
    if (! socket) {
        return std::unexpected(std::move(socket).error());
    }
    std::string temporary;
    for (int tried = 1;; ++tried) {
        temporary = temporary_name_for(name);
        const auto temporary_address = address_of(temporary);
        if (! temporary_address) {
            return std::unexpected(error{ std::errc::filename_too_long, name });
        }
        if (::bind(socket->get(), as_socket_address(*temporary_address), sizeof(*temporary_address)) == 0) {
            break;
        }
        // A file there may be another process's, so another name is tried
        // rather than the file removed.
        if ((errno != EADDRINUSE) || (tried == temporary_name_tries)) {
            return std::unexpected(last_error("bind"));
        }
    }
    struct stat bound { };
    if (::lstat(temporary.c_str(), &bound) != 0) {
        return std::unexpected(last_error("lstat"));
    }
    // From here the destructor removes the file at _name on failure.
    unix_listener made{ std::move(*socket), temporary, bound.st_dev, bound.st_ino };
    if (::chmod(temporary.c_str(), S_IRUSR | S_IWUSR) != 0) {
        return std::unexpected(last_error("chmod"));
    }
    if (::listen(made._socket.get(), SOMAXCONN) != 0) {
        return std::unexpected(last_error("listen"));
    }
    if (auto placed = place(temporary, name, *address); ! placed) {
        return std::unexpected(std::move(placed).error());
    }
    made._name = std::move(name);
    if (::unlink(temporary.c_str()) != 0) {
        return std::unexpected(last_error("unlink"));
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
