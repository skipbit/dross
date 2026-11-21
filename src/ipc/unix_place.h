#pragma once

#include "dross/ipc/message_acceptor.h"
#include "dross/ipc/message_connector.h"
#include "dross/platform/path.h"
#include "dross/type/error.h"

#include <expected>
#include <memory>

namespace dross::ipc {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_UNIX_PLACE_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_UNIX_PLACE_HIDDEN
#endif

// The part behind unix_socket: unix_transport, seen through the public
// shape. Each acceptor has one transport for every connection it takes;
// each connection a connector makes has a transport of its own, which lives
// until the connection ends.

// Listens at location, as unix_transport::listen() does.
DROSS_UNIX_PLACE_HIDDEN std::expected<std::unique_ptr<message_acceptor>, error> listen_at(const path& location, std::shared_ptr<message_acceptor_events> events);

// Connects to location, as unix_transport::connect() does. A try waits for
// the peer to take the connection, and fails with the reason the peer was
// refused, such as peer_not_same_user.
DROSS_UNIX_PLACE_HIDDEN std::shared_ptr<message_connector> connector_to(const path& location);

#undef DROSS_UNIX_PLACE_HIDDEN

}  // namespace dross::ipc
