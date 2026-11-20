#pragma once

#include "dross/platform/path.h"
#include "dross/type/error.h"
#include "notification/backoff.h"
#include "notification/transport.h"

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_CONNECTOR_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_CONNECTOR_HIDDEN
#endif

class unix_transport;

// What a connector tells the layer above it. Every call comes on the
// transport's thread, one at a time.
class DROSS_CONNECTOR_HIDDEN connector_events {
public:
    virtual ~connector_events() = default;

    virtual void connected() = 0;
    virtual void received(std::vector<std::byte> message) = 0;

    // Follows connected(), once. reason is an empty error when the peer
    // ended the connection in order.
    virtual void disconnected(error reason) = 0;
};

// Keeps one connection to a transport listening at a path. A thread of its
// own connects, and once a try fails or the connection ends, tries again
// after a wait that doubles from 100 milliseconds up to 5 seconds and starts
// over once a connection is made. A try that fails is not reported, nor is a
// peer refused before it connected.
//
// It holds the layer above without keeping it going, and keeps it going for
// the length of each event, so the layer above may let itself go from inside
// any of them. It may let the connector go from inside connected() or
// received(), but not disconnected(), after which the connector still runs.
class DROSS_CONNECTOR_HIDDEN connector final : private transport_events {
public:
    connector(path where, std::weak_ptr<connector_events> above);

    ~connector() override;

    connector(const connector& other) = delete;
    connector& operator=(const connector& other) = delete;
    connector(connector&& other) = delete;
    connector& operator=(connector&& other) = delete;

    // Starts the thread, once.
    void start();

    // Queues message for the peer; dropped while not connected.
    void send(std::span<const std::byte> message);

    // Ends the connection, which disconnected() reports with reason.
    void close(error reason);

private:
    void connected(connection_id id) override;
    void received(connection_id id, std::vector<std::byte> message) override;
    void closed(connection_id id, error reason) override;
    void stopped(error reason) override;

    void run();

    const path _where;
    const std::weak_ptr<connector_events> _above;

    std::mutex _lock;
    std::condition_variable _wake;
    std::unique_ptr<unix_transport> _transport;
    std::optional<connection_id> _connection;
    std::optional<error> _closing;
    backoff _waits;
    bool _stopping{ false };

    std::thread _thread;
};

#undef DROSS_CONNECTOR_HIDDEN

}  // namespace dross
