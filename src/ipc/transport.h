#pragma once

#include "dross/type/error.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_TRANSPORT_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_TRANSPORT_HIDDEN
#endif

// Names one connection of a transport. Numbers are not reused within a
// transport.
using connection_id = std::uint64_t;

// What a transport tells the layer above it. Every call comes on the
// transport's own thread, one at a time, and may call back into the
// transport.
//
// Each connection gets connected() first and closed() last, exactly once,
// also when the layer above closed it. A peer that is refused before it is
// connected gets closed() alone. Once the transport is destroyed, nothing
// more is called.
class DROSS_TRANSPORT_HIDDEN transport_events {
public:
    virtual ~transport_events() = default;

    virtual void connected(connection_id id) = 0;
    virtual void received(connection_id id, std::vector<std::byte> message) = 0;

    // reason is an empty error when the connection ended in order.
    virtual void closed(connection_id id, error reason) = 0;

    // The transport's thread ended on a failure of its own, after closing
    // every connection with that reason and what it listened on. Nothing is
    // called after it. Not called when the transport is destroyed.
    virtual void stopped(error reason) = 0;
};

// Carries messages between processes. Safe to call from any thread.
class DROSS_TRANSPORT_HIDDEN transport {
public:
    virtual ~transport() = default;

    // Queues a message to be sent. A message larger than 64 KiB fails with
    // message_too_large. A message that would leave more than 1 MiB waiting
    // fails with queue_overflow, and closes the connection with that
    // reason. A connection that is not open fails with not_connected.
    virtual std::expected<void, error> send(connection_id id, std::span<const std::byte> message) = 0;

    // Closes a connection, dropping what is still waiting to be sent.
    virtual void close(connection_id id) = 0;
};

#undef DROSS_TRANSPORT_HIDDEN

}  // namespace dross
