#pragma once

#include "dross/ipc/message_connector.h"
#include "dross/type/error.h"

#include <cstddef>
#include <memory>
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

// What a connector tells the layer above it, one call at a time: from the
// connector's thread or from the part's.
class DROSS_CONNECTOR_HIDDEN connector_events {
public:
    virtual ~connector_events() = default;

    virtual void connected() = 0;
    virtual void received(std::vector<std::byte> message) = 0;

    // Follows connected(), once. reason is an empty error when the peer
    // ended the connection in order.
    virtual void disconnected(error reason) = 0;
};

// Keeps one connection to a place through a part's connector. A thread of
// its own connects, and once a try fails or the connection ends, tries again
// after a wait that doubles from 100 milliseconds up to 5 seconds and starts
// over once a connection is made. A try that fails is not reported.
//
// What the part tells before its connect() has returned is held, and told
// after connected(). disconnected() is told before the next try starts.
//
// It holds the layer above without keeping it going, and keeps it going for
// the length of each event, so the layer above may let itself go from inside
// any of them. It may let the connector go from inside connected() or
// received(), but not disconnected(), after which the connector still runs.
// Once the connector is gone, nothing more is told.
class DROSS_CONNECTOR_HIDDEN connector final {
public:
    connector(std::shared_ptr<ipc::message_connector> part, std::weak_ptr<connector_events> above);

    ~connector();

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
    class state;
    class attempt;

    static void run(const std::shared_ptr<state>& shared, const std::shared_ptr<ipc::message_connector>& part, const std::weak_ptr<connector_events>& above);

    const std::shared_ptr<ipc::message_connector> _part;
    const std::weak_ptr<connector_events> _above;
    const std::shared_ptr<state> _state;
    std::thread _thread;
};

#undef DROSS_CONNECTOR_HIDDEN

}  // namespace dross
