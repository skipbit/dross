#include "notification/publisher.h"

#include "notification/unix_transport.h"
#include "notification/wire.h"

#include <utility>
#include <variant>
#include <vector>

namespace dross {

std::expected<std::unique_ptr<publisher>, error> publisher::listen(const path& where)
{
    std::unique_ptr<publisher> made{ new publisher{ where } };
    auto transport = unix_transport::listen(where, *made);
    if (! transport) {
        return std::unexpected(std::move(transport).error());
    }
    made->_transport = std::move(*transport);
    made->_transport->start();
    return made;
}

publisher::publisher(path where)
    : _where{ std::move(where) }
{
}

publisher::~publisher()
{
    // Stops the thread before the state its events touch goes.
    std::shared_ptr<unix_transport> ending;
    {
        const std::lock_guard guard{ _lock };
        _stopping = true;
        ending = std::move(_transport);
    }
}

bool publisher::wanted(const std::string_view name) const
{
    const std::lock_guard guard{ _lock };
    return _subscribers.wanted(name);
}

void publisher::send(const std::string_view name, const value& content)
{
    std::vector<connection_id> receivers;
    std::shared_ptr<unix_transport> through;
    {
        const std::lock_guard guard{ _lock };
        receivers = _subscribers.receivers(name);
        through = _transport;
    }
    if (receivers.empty() || ! through) {
        return;
    }
    const auto bytes = wire::encode(wire::notice{ std::string{ name }, content });
    // A connection that cannot take the message is closed by the
    // transport, and its listener connects again.
    for (const connection_id id : receivers) {
        (void)through->send(id, bytes);
    }
}

void publisher::connected(const connection_id id)
{
    const std::lock_guard guard{ _lock };
    _subscribers.join(id);
}

void publisher::received(const connection_id id, std::vector<std::byte> message)
{
    auto read = wire::decode(message);
    if (read && ! std::holds_alternative<wire::notice>(*read)) {
        const std::lock_guard guard{ _lock };
        if (const auto* asked = std::get_if<wire::subscribe>(&*read)) {
            _subscribers.add(id, asked->name);
        } else {
            _subscribers.remove(id, std::get<wire::unsubscribe>(*read).name);
        }
        return;
    }
    // A listener sends nothing else, so the peer is not one.
    if (const auto through = transport()) {
        through->close(id);
    }
}

void publisher::closed(const connection_id id, error)
{
    const std::lock_guard guard{ _lock };
    _subscribers.leave(id);
}

void publisher::stopped(error)
{
    // Called on the stopped transport's own thread, which lets it go once
    // this returns.
    auto again = unix_transport::listen(_where, *this);
    std::shared_ptr<unix_transport> replaced;
    const std::lock_guard guard{ _lock };
    replaced = std::move(_transport);
    if (! again || _stopping) {
        return;
    }
    _transport = std::move(*again);
    // Inside the lock: once the destructor has taken _transport, it no
    // longer waits for this thread, so nothing of this is touched after the
    // lock is let go.
    _transport->start();
}

std::shared_ptr<unix_transport> publisher::transport() const
{
    const std::lock_guard guard{ _lock };
    return _transport;
}

}  // namespace dross
