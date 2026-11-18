#pragma once

#include "dross/platform/path.h"
#include "dross/type/error.h"

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <sys/types.h>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_UNIX_SOCKET_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_UNIX_SOCKET_HIDDEN
#endif

// A file descriptor that is closed when this goes away.
class DROSS_UNIX_SOCKET_HIDDEN socket_handle final {
public:
    socket_handle() noexcept = default;
    explicit socket_handle(int fd) noexcept;

    socket_handle(socket_handle&& other) noexcept;
    socket_handle& operator=(socket_handle&& other) noexcept;
    ~socket_handle();

    socket_handle(const socket_handle& other) = delete;
    socket_handle& operator=(const socket_handle& other) = delete;

    // -1 when there is none.
    int get() const noexcept;

private:
    int _fd{ -1 };
};

// A Unix domain stream socket listening at a path in the file system. The
// socket file is made readable and writable by its owner alone.
//
// The socket is made and listens under a name of its own next to the path,
// the path with a dot, the process id, a dot and eight hexadecimal digits
// after it, and then appears at the path; a path that leaves no room for
// that name fails with filename_too_long. A process that ends while it
// listens under that name leaves the file behind; no listener removes it.
//
// A file at the path that is not a socket, or a socket that a process is
// listening on, makes listen() fail with address_in_use, so of two
// processes that start at the same moment one takes the path.
//
// A socket file left by a process that has gone refuses connections, and
// is replaced. Finding that out connects to the file once, so a listener
// there sees a connection that closes at once. Two processes that find
// the same old file at the same moment can both succeed; the path then
// leads to the one that replaced it last.
//
// On destruction the socket file is removed, unless another process has
// put its own file at the path in the meantime.
class DROSS_UNIX_SOCKET_HIDDEN unix_listener final {
public:
    static std::expected<unix_listener, error> listen(const path& location);

    unix_listener(unix_listener&& other) noexcept = default;
    ~unix_listener();

    unix_listener(const unix_listener& other) = delete;
    unix_listener& operator=(const unix_listener& other) = delete;
    unix_listener& operator=(unix_listener&& other) = delete;

    int fd() const noexcept;

    // A new connection, or nothing when none is waiting.
    std::expected<std::optional<socket_handle>, error> accept();

private:
    unix_listener(socket_handle socket, std::string name, dev_t device, ino_t inode) noexcept;

    socket_handle _socket;
    std::string _name;
    dev_t _device{};
    ino_t _inode{};
};

// Receives what has arrived without waiting: nothing when no byte is there
// yet, and 0 bytes when the peer has closed the connection.
DROSS_UNIX_SOCKET_HIDDEN std::expected<std::optional<std::size_t>, error> receive_some(int fd, std::span<std::byte> into);

// Connects to a socket listening at location. The connection does not wait:
// a listener with no room left fails it with resource_unavailable_try_again.
DROSS_UNIX_SOCKET_HIDDEN std::expected<socket_handle, error> unix_connect(const path& location);

// What differs between platforms; each has its own file,
// unix_socket_linux.cpp and unix_socket_macos.cpp. Every socket these
// return is non-blocking, is closed on exec, and does not raise SIGPIPE.

DROSS_UNIX_SOCKET_HIDDEN std::expected<socket_handle, error> open_unix_socket();

// A new connection on a listening socket, or nothing when none is waiting.
DROSS_UNIX_SOCKET_HIDDEN std::expected<std::optional<socket_handle>, error> accept_unix_socket(int listening);

// The user at the other end of a connected socket.
DROSS_UNIX_SOCKET_HIDDEN std::expected<uid_t, error> peer_uid(int fd);

// Sends what fits without waiting, which is 0 bytes when the socket is
// full. A peer that has gone fails it with broken_pipe.
DROSS_UNIX_SOCKET_HIDDEN std::expected<std::size_t, error> send_some(int fd, std::span<const std::byte> bytes);

#undef DROSS_UNIX_SOCKET_HIDDEN

}  // namespace dross
