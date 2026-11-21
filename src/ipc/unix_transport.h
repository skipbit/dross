#pragma once

#include "dross/platform/path.h"
#include "dross/type/error.h"
#include "ipc/transport.h"

#include <expected>
#include <memory>
#include <span>
#include <sys/types.h>
#include <thread>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_UNIX_TRANSPORT_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_UNIX_TRANSPORT_HIDDEN
#endif

// A transport over Unix domain stream sockets, with a thread of its own
// that waits on the sockets and calls the events. A peer whose effective
// user differs from this process's is refused with peer_not_same_user.
//
// listen() and connect() return before the thread starts, and no event
// comes until start() is called, so the caller holds the transport first.
//
// A connection it fails to accept, as when the process has no file
// descriptor left, is not reported: accepting pauses, for a wait that
// doubles from 100 milliseconds up to 5 seconds while it keeps failing, so
// the thread does not spin. macOS drops the waiting connection the failed
// accept was for; Linux leaves it to be accepted later. A failure of the
// waiting itself ends the thread and is reported by stopped().
//
// Destroyed from another thread, it stops its thread and waits for it.
// Destroyed from inside one of its own events, it lets its thread go: the
// thread calls nothing more once that event returns, and closes the
// sockets on its way out.
class DROSS_UNIX_TRANSPORT_HIDDEN unix_transport final : public transport {
public:
    // Listens at location and takes every peer of the same user that
    // connects. See unix_listener for a file already at the path.
    static std::expected<std::unique_ptr<unix_transport>, error> listen(const path& location, transport_events& events);

    // Makes one connection to a transport of the same user listening at
    // location; its connected() is the first event.
    static std::expected<std::unique_ptr<unix_transport>, error> connect(const path& location, transport_events& events);

    // Like the above, but accepts only peers running as allowed.
    static std::expected<std::unique_ptr<unix_transport>, error> listen(const path& location, transport_events& events, uid_t allowed);

    // Like the above, but connects only to a peer running as allowed.
    static std::expected<std::unique_ptr<unix_transport>, error> connect(const path& location, transport_events& events, uid_t allowed);

    // Starts the thread, once.
    void start();

    ~unix_transport() override;

    unix_transport(const unix_transport& other) = delete;
    unix_transport& operator=(const unix_transport& other) = delete;
    unix_transport(unix_transport&& other) = delete;
    unix_transport& operator=(unix_transport&& other) = delete;

    std::expected<void, error> send(connection_id id, std::span<const std::byte> message) override;
    void close(connection_id id) override;

private:
    class state;

    explicit unix_transport(std::shared_ptr<state> shared);

    std::shared_ptr<state> _state;
    std::thread _thread;
};

#undef DROSS_UNIX_TRANSPORT_HIDDEN

}  // namespace dross
