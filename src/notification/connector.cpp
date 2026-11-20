#include "notification/connector.h"

#include "notification/unix_transport.h"

#include <utility>

namespace dross {

connector::connector(path where, std::weak_ptr<connector_events> above)
    : _where{ std::move(where) }
    , _above{ std::move(above) }
{
}

connector::~connector()
{
    std::unique_ptr<unix_transport> ending;
    {
        const std::lock_guard guard{ _lock };
        _stopping = true;
        ending = std::move(_transport);
        _connection.reset();
    }
    _wake.notify_all();
    // Waits for the transport's thread, unless this runs inside one of its
    // events.
    ending.reset();
    if (_thread.joinable()) {
        _thread.join();
    }
}

void connector::start()
{
    _thread = std::thread{ [this] {
        run();
    } };
}

void connector::send(const std::span<const std::byte> message)
{
    const std::lock_guard guard{ _lock };
    if (! _connection) {
        return;
    }
    // A message over 64 KiB is not sent and the connection stays; one that
    // overflows the queue closes it, and it is then made again.
    (void)_transport->send(*_connection, message);
}

void connector::close(error reason)
{
    const std::lock_guard guard{ _lock };
    if (! _connection) {
        return;
    }
    _closing = std::move(reason);
    _transport->close(*_connection);
}

void connector::connected(const connection_id id)
{
    const auto above = _above.lock();
    {
        const std::lock_guard guard{ _lock };
        _connection = id;
        _waits.reset();
    }
    if (above) {
        above->connected();
    }
}

void connector::received(connection_id, std::vector<std::byte> message)
{
    if (const auto above = _above.lock()) {
        above->received(std::move(message));
    }
}

void connector::closed(connection_id, error reason)
{
    // Taken first, so it is let go last: letting it go may destroy this.
    const auto above = _above.lock();
    bool was_connected = false;
    {
        const std::lock_guard guard{ _lock };
        was_connected = _connection.has_value();
        _connection.reset();
        if (_closing) {
            reason = std::move(*_closing);
            _closing.reset();
        }
    }
    // Reported before run() is woken, so the next connection's events follow
    // it. The transport stays in _transport meanwhile, so the destructor
    // waits for this thread.
    if (above && was_connected) {
        above->disconnected(std::move(reason));
    }
    std::unique_ptr<unix_transport> ending;
    {
        const std::lock_guard guard{ _lock };
        ending = std::move(_transport);
        // Inside the lock: once the transport is out of _transport, the
        // destructor no longer waits for this thread, so nothing of this is
        // touched after the lock is let go.
        _wake.notify_all();
    }
    // ending goes here, inside its own event, so its thread is let go.
}

void connector::stopped(error)
{
    std::unique_ptr<unix_transport> ending;
    {
        const std::lock_guard guard{ _lock };
        ending = std::move(_transport);
        _connection.reset();
        _wake.notify_all();
    }
}

void connector::run()
{
    std::unique_lock guard{ _lock };
    while (true) {
        guard.unlock();
        auto made = unix_transport::connect(_where, *this);
        guard.lock();
        if (_stopping) {
            return;
        }
        if (made) {
            _transport = std::move(*made);
            _transport->start();
            _wake.wait(guard, [this] {
                return _stopping || ! _transport;
            });
            if (_stopping) {
                return;
            }
        }
        if (_wake.wait_for(guard, _waits.next(), [this] {
            return _stopping;
        })) {
            return;
        }
    }
}

}  // namespace dross
