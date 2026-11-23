#pragma once

#include "dross/ipc/message_acceptor.h"
#include "dross/ipc/message_connector.h"
#include "dross/platform/path.h"
#include "dross/type/error.h"

#include <expected>
#include <memory>
#include <utility>

namespace dross::ipc {

/**
 * @brief A place where processes meet: a Unix domain socket at a path.
 *
 * What listens there and what connects to it is decided by what the place
 * is given to, such as dross::broadcaster::create() and
 * dross::listener::create():
 * @code
 * int main()
 * {
 *     const dross::ipc::unix_socket place{ dross::path{ std::string{ "/run/user/1000/player.sock" } } };
 *     return place.location().string().empty() ? 1 : 0;
 * }
 * @endcode
 *
 * The path is not checked here. A path the system cannot listen on or
 * connect to fails where the place is used.
 *
 * Connections:
 * - Only processes of the same effective user connect. The socket file is
 *   made for its owner alone, and removed when the acceptor goes, unless
 *   another process has put its own file at the path in the meantime
 * - A message is at most 64 KiB, and at most 1 MiB waits to be sent on one
 *   connection. A larger message fails with ipc_errc::message_too_large;
 *   one that would leave more waiting fails with ipc_errc::queue_overflow
 *   and closes the connection
 * - When a connection cannot be accepted, as when the process has no file
 *   descriptor left, accepting pauses, for a wait that doubles from 100
 *   milliseconds up to 5 seconds while it keeps failing. macOS drops the
 *   connection it could not accept
 * - Each acceptor and each connection runs a thread of its own, from which
 *   it calls the events
 */
class unix_socket final {
public:
    /**
     * @brief Construct the place at location.
     * @param location The path of the socket file
     */
    explicit unix_socket(path location)
        : _location{ std::move(location) }
    {
    }

    /**
     * @brief Get the path of the socket file.
     * @return The path the place was made with
     */
    const path& location() const noexcept
    {
        return _location;
    }

    /**
     * @brief Listen at the path, taking every peer of the same user.
     * @param events What to tell about the connections
     * @return What listens, for as long as it is held, or why listening
     *         failed, such as std::errc::address_in_use when another
     *         process listens there, or
     *         std::errc::resource_unavailable_try_again when the system
     *         will not start the thread that waits on the socket, or
     *         std::errc::not_enough_memory when there is no memory to
     *         start it
     */
    std::expected<std::unique_ptr<message_acceptor>, error> listen(std::shared_ptr<message_acceptor_events> events) const;

    /**
     * @brief Get what connects to the path.
     *
     * A try waits for the peer to take the connection. It fails with the
     * system's error, such as std::errc::no_such_file_or_directory when
     * there is no socket file and std::errc::connection_refused when
     * nothing listens at it, and with ipc_errc::peer_not_same_user when
     * another user does. Each connection has a thread that waits on its
     * socket; the try fails with std::errc::resource_unavailable_try_again
     * when the system will not start it, and with
     * std::errc::not_enough_memory when there is no memory to start it.
     *
     * @return The connector
     */
    std::shared_ptr<message_connector> connector() const;

private:
    path _location;
};

}  // namespace dross::ipc
