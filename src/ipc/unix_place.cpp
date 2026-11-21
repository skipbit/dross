#include "ipc/unix_place.h"

#include "dross/ipc/message_channel.h"
#include "dross/ipc/unix_socket.h"
#include "ipc/transport.h"
#include "ipc/unix_transport.h"

#include <condition_variable>
#include <cstddef>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

namespace dross::ipc {

namespace {

// What a channel sends through: the transport its connection belongs to.
class channel_owner {
public:
    virtual ~channel_owner() = default;

    virtual std::expected<void, error> send(connection_id id, std::span<const std::byte> message) = 0;
    virtual void close(connection_id id) = 0;
};

class unix_channel final : public message_channel {
public:
    unix_channel(std::shared_ptr<channel_owner> owner, connection_id id)
        : _owner{ std::move(owner) }
        , _id{ id }
    {
    }

    std::expected<void, error> send(std::span<const std::byte> message) override
    {
        return _owner->send(_id, message);
    }

    void close() override
    {
        _owner->close(_id);
    }

private:
    const std::shared_ptr<channel_owner> _owner;
    const connection_id _id;
};

// The transport of an acceptor, and the events of what it accepted. Every
// event keeps the core going for its length, so the acceptor may be
// destroyed from inside one.
class acceptor_core final : public channel_owner, private transport_events, public std::enable_shared_from_this<acceptor_core> {
public:
    explicit acceptor_core(std::shared_ptr<message_acceptor_events> above)
        : _above{ std::move(above) }
    {
    }

    std::expected<void, error> listen(const path& location)
    {
        auto made = unix_transport::listen(location, *this);
        if (! made) {
            return std::unexpected(made.error());
        }
        unix_transport* started = made->get();
        {
            const std::lock_guard guard{ _lock };
            _transport = std::move(*made);
        }
        started->start();
        return {};
    }

    // Destroys the transport, then lets the events go.
    void stop()
    {
        std::unique_ptr<unix_transport> ending;
        {
            const std::lock_guard guard{ _lock };
            ending = std::move(_transport);
        }
        ending.reset();
        const std::lock_guard guard{ _lock };
        _channels.clear();
        _above.reset();
    }

    std::expected<void, error> send(const connection_id id, const std::span<const std::byte> message) override
    {
        const std::lock_guard guard{ _lock };
        if (! _transport) {
            return std::unexpected(error{ std::errc::not_connected });
        }
        return _transport->send(id, message);
    }

    void close(const connection_id id) override
    {
        const std::lock_guard guard{ _lock };
        if (_transport) {
            _transport->close(id);
        }
    }

private:
    void connected(const connection_id id) override
    {
        const auto self = shared_from_this();
        std::shared_ptr<message_acceptor_events> above;
        {
            const std::lock_guard guard{ _lock };
            above = _above;
        }
        if (! above) {
            return;
        }
        auto events = above->accepted(std::make_shared<unix_channel>(self, id));
        const std::lock_guard guard{ _lock };
        _channels[id] = std::move(events);
    }

    void received(const connection_id id, std::vector<std::byte> message) override
    {
        const auto self = shared_from_this();
        const auto events = find(id);
        if (events) {
            events->received(std::move(message));
        }
    }

    void closed(const connection_id id, error reason) override
    {
        const auto self = shared_from_this();
        std::shared_ptr<message_channel_events> events;
        {
            const std::lock_guard guard{ _lock };
            const auto found = _channels.find(id);
            if (found == _channels.end()) {
                return;
            }
            events = std::move(found->second);
            _channels.erase(found);
        }
        events->closed(std::move(reason));
    }

    void stopped(error reason) override
    {
        const auto self = shared_from_this();
        std::shared_ptr<message_acceptor_events> above;
        {
            const std::lock_guard guard{ _lock };
            above = _above;
        }
        if (above) {
            above->stopped(std::move(reason));
        }
    }

    std::shared_ptr<message_channel_events> find(const connection_id id)
    {
        const std::lock_guard guard{ _lock };
        const auto found = _channels.find(id);
        return found == _channels.end() ? nullptr : found->second;
    }

    std::mutex _lock;
    std::shared_ptr<message_acceptor_events> _above;
    std::unique_ptr<unix_transport> _transport;
    std::map<connection_id, std::shared_ptr<message_channel_events>> _channels;
};

class unix_acceptor final : public message_acceptor {
public:
    explicit unix_acceptor(std::shared_ptr<acceptor_core> core)
        : _core{ std::move(core) }
    {
    }

    ~unix_acceptor() override
    {
        _core->stop();
    }

    unix_acceptor(const unix_acceptor& other) = delete;
    unix_acceptor& operator=(const unix_acceptor& other) = delete;

private:
    const std::shared_ptr<acceptor_core> _core;
};

// One connection made to a place, with its own transport. It keeps itself
// going until the connection ends, so letting its channel go does not end
// it; the transport is then destroyed from inside its last event.
class connection_core final : public channel_owner, private transport_events, public std::enable_shared_from_this<connection_core> {
public:
    explicit connection_core(std::shared_ptr<message_channel_events> events)
        : _events{ std::move(events) }
    {
    }

    // Makes the connection and waits for the peer to take it.
    std::expected<std::shared_ptr<message_channel>, error> connect(const path& location)
    {
        auto made = unix_transport::connect(location, *this);
        if (! made) {
            return std::unexpected(made.error());
        }
        unix_transport* started = made->get();
        {
            const std::lock_guard guard{ _lock };
            _transport = std::move(*made);
            _keep = shared_from_this();
        }
        started->start();

        std::unique_lock guard{ _lock };
        _settled.wait(guard, [this] {
            return _id.has_value() || _refused.has_value();
        });
        if (! _id) {
            auto ending = std::move(_transport);
            _keep.reset();
            guard.unlock();
            ending.reset();
            return std::unexpected(*_refused);
        }
        return std::make_shared<unix_channel>(shared_from_this(), *_id);
    }

    std::expected<void, error> send(const connection_id id, const std::span<const std::byte> message) override
    {
        const std::lock_guard guard{ _lock };
        if (! _transport) {
            return std::unexpected(error{ std::errc::not_connected });
        }
        return _transport->send(id, message);
    }

    void close(const connection_id id) override
    {
        const std::lock_guard guard{ _lock };
        if (_transport) {
            _transport->close(id);
        }
    }

private:
    void connected(const connection_id id) override
    {
        {
            const std::lock_guard guard{ _lock };
            _id = id;
        }
        _settled.notify_all();
    }

    void received(connection_id, std::vector<std::byte> message) override
    {
        const auto self = shared_from_this();
        _events->received(std::move(message));
    }

    void closed(connection_id, error reason) override
    {
        const auto self = shared_from_this();
        bool was_connected = false;
        {
            const std::lock_guard guard{ _lock };
            was_connected = _id.has_value();
            if (! was_connected) {
                _refused = std::move(reason);
            }
        }
        if (! was_connected) {
            _settled.notify_all();
            return;
        }
        auto events = std::move(_events);
        events->closed(std::move(reason));
        const std::lock_guard guard{ _lock };
        _keep.reset();
    }

    void stopped(error reason) override
    {
        {
            const std::lock_guard guard{ _lock };
            if (! _id && ! _refused) {
                _refused = std::move(reason);
            }
        }
        _settled.notify_all();
    }

    std::mutex _lock;
    std::condition_variable _settled;
    std::shared_ptr<message_channel_events> _events;
    std::unique_ptr<unix_transport> _transport;
    std::shared_ptr<connection_core> _keep;
    std::optional<connection_id> _id;
    std::optional<error> _refused;
};

class unix_connector final : public message_connector {
public:
    explicit unix_connector(path location)
        : _location{ std::move(location) }
    {
    }

    std::expected<std::shared_ptr<message_channel>, error> connect(std::shared_ptr<message_channel_events> events) override
    {
        return std::make_shared<connection_core>(std::move(events))->connect(_location);
    }

private:
    const path _location;
};

}  // namespace

std::expected<std::unique_ptr<message_acceptor>, error> listen_at(const path& location, std::shared_ptr<message_acceptor_events> events)
{
    auto core = std::make_shared<acceptor_core>(std::move(events));
    if (auto listening = core->listen(location); ! listening) {
        return std::unexpected(listening.error());
    }
    return std::make_unique<unix_acceptor>(std::move(core));
}

std::shared_ptr<message_connector> connector_to(const path& location)
{
    return std::make_shared<unix_connector>(location);
}

std::expected<std::unique_ptr<message_acceptor>, error> unix_socket::listen(std::shared_ptr<message_acceptor_events> events) const
{
    return listen_at(_location, std::move(events));
}

std::shared_ptr<message_connector> unix_socket::connector() const
{
    return connector_to(_location);
}

}  // namespace dross::ipc
