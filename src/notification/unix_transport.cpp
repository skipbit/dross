#include "notification/unix_transport.h"

#include "dross/notification/error.h"
#include "notification/frame.h"
#include "notification/outbox.h"
#include "notification/poller.h"
#include "notification/unix_socket.h"

#include <atomic>
#include <map>
#include <mutex>
#include <optional>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dross {

namespace {

// One connection. The reader and what the poller watches belong to the
// thread of the transport; the outbox and ending are shared under the lock.
struct connection final {
    explicit connection(socket_handle accepted)
        : socket{ std::move(accepted) }
    {
    }

    socket_handle socket;
    frame_reader reader;
    outbox waiting;
    poller::interest watched{ poller::interest::read };
    bool announced{ false };
    // Set when the connection is to be closed, with the reason.
    std::optional<error> ending;
};

// Whether the peer runs as the same user as this process.
std::optional<error> refusal_of(const int fd)
{
    const auto user = peer_uid(fd);
    if (! user) {
        return user.error();
    }
    if (*user != ::getuid()) {
        return error{ notification_errc::peer_not_same_user };
    }
    return std::nullopt;
}

}  // namespace

class unix_transport::state final {
public:
    state(transport_events& events, poller watcher, std::optional<unix_listener> listening)
        : _events{ events }
        , _poller{ std::move(watcher) }
        , _listening{ std::move(listening) }
        , _buffer(max_message_size)
    {
    }

    // Before the thread starts, or on it.
    void adopt(socket_handle socket)
    {
        const int fd = socket.get();
        connection made{ std::move(socket) };
        made.ending = refusal_of(fd);
        if (! made.ending) {
            if (auto added = _poller.add(fd, poller::interest::read); ! added) {
                made.ending = std::move(added).error();
            }
        }
        const connection_id id = _next_id++;
        _ids_by_fd[fd] = id;
        const std::lock_guard guard{ _lock };
        _connections.emplace(id, std::move(made));
    }

    std::expected<void, error> send(const connection_id id, const std::span<const std::byte> message)
    {
        auto framed = frame_message(message);
        if (! framed) {
            return std::unexpected(std::move(framed).error());
        }
        {
            const std::lock_guard guard{ _lock };
            const auto found = _connections.find(id);
            if ((found == _connections.end()) || found->second.ending) {
                return std::unexpected(error{ std::errc::not_connected });
            }
            if (auto pushed = found->second.waiting.push(std::move(*framed)); ! pushed) {
                found->second.ending = pushed.error();
                _poller.wake();
                return pushed;
            }
        }
        _poller.wake();
        return {};
    }

    void close(const connection_id id)
    {
        {
            const std::lock_guard guard{ _lock };
            const auto found = _connections.find(id);
            if ((found == _connections.end()) || found->second.ending) {
                return;
            }
            found->second.ending = error{};
        }
        _poller.wake();
    }

    void stop() noexcept
    {
        _stopping = true;
        _poller.wake();
    }

    void run()
    {
        while (settle()) {
            const auto ready = _poller.wait();
            if (! ready) {
                end_all(ready.error());
                settle();
                return;
            }
            for (const auto& each : *ready) {
                if (_stopping) {
                    return;
                }
                if (_listening && (each.fd == _listening->fd())) {
                    accept_waiting();
                } else {
                    serve(each);
                }
            }
        }
    }

private:
    // Closes the connections that are ending, announces the new ones and
    // watches for writing where bytes wait. Returns false once stopping.
    bool settle()
    {
        std::vector<connection_id> fresh;
        std::vector<std::pair<connection_id, error>> ended;
        {
            const std::lock_guard guard{ _lock };
            for (auto it = _connections.begin(); it != _connections.end();) {
                connection& each = it->second;
                const int fd = each.socket.get();
                if (each.ending) {
                    _poller.remove(fd);
                    _ids_by_fd.erase(fd);
                    ended.emplace_back(it->first, std::move(*each.ending));
                    it = _connections.erase(it);
                    continue;
                }
                if (! each.announced) {
                    each.announced = true;
                    fresh.push_back(it->first);
                }
                const auto wanted = each.waiting.empty() ? poller::interest::read : poller::interest::read_write;
                if (wanted != each.watched) {
                    if (auto changed = _poller.change(fd, wanted); ! changed) {
                        each.ending = std::move(changed).error();
                        _poller.wake();
                    } else {
                        each.watched = wanted;
                    }
                }
                ++it;
            }
        }
        for (const connection_id id : fresh) {
            if (_stopping) {
                return false;
            }
            _events.connected(id);
        }
        for (auto& [id, reason] : ended) {
            if (_stopping) {
                return false;
            }
            _events.closed(id, std::move(reason));
        }
        return ! _stopping;
    }

    void accept_waiting()
    {
        while (true) {
            auto accepted = _listening->accept();
            if (! accepted || ! *accepted) {
                return;
            }
            adopt(std::move(**accepted));
        }
    }

    void serve(const poller::event& ready)
    {
        const auto known = _ids_by_fd.find(ready.fd);
        if (known == _ids_by_fd.end()) {
            return;
        }
        const connection_id id = known->second;
        connection* const served = open_connection(id);
        if (served == nullptr) {
            return;
        }
        if (ready.readable || ready.hangup) {
            receive(id, *served);
        }
        if (ready.writable) {
            const std::lock_guard guard{ _lock };
            if (! served->ending) {
                if (auto flushed = served->waiting.flush(served->socket.get()); ! flushed) {
                    served->ending = std::move(flushed).error();
                }
            }
        }
    }

    // The connection, when it is not ending. Only this thread removes
    // connections, so the pointer stays valid until the next settle().
    connection* open_connection(const connection_id id)
    {
        const std::lock_guard guard{ _lock };
        const auto found = _connections.find(id);
        if ((found == _connections.end()) || found->second.ending) {
            return nullptr;
        }
        return &found->second;
    }

    void receive(const connection_id id, connection& from)
    {
        const auto got = receive_some(from.socket.get(), _buffer);
        if (! got) {
            end(from, got.error());
            return;
        }
        if (! *got) {
            return;
        }
        if (**got == 0) {
            end(from, from.reader.holds_partial() ? error{ notification_errc::message_cut_short } : error{});
            return;
        }
        auto messages = from.reader.take(std::span{ _buffer }.first(**got));
        if (! messages) {
            end(from, messages.error());
            return;
        }
        for (auto& message : *messages) {
            if (_stopping || (open_connection(id) == nullptr)) {
                return;
            }
            _events.received(id, std::move(message));
        }
    }

    // The first reason given is the one reported.
    void end(connection& which, error reason)
    {
        const std::lock_guard guard{ _lock };
        if (! which.ending) {
            which.ending = std::move(reason);
        }
    }

    void end_all(const error& reason)
    {
        const std::lock_guard guard{ _lock };
        for (auto& [id, each] : _connections) {
            if (! each.ending) {
                each.ending = reason;
            }
        }
    }

    transport_events& _events;
    poller _poller;
    std::optional<unix_listener> _listening;
    std::vector<std::byte> _buffer;
    std::unordered_map<int, connection_id> _ids_by_fd;
    connection_id _next_id{ 1 };
    std::atomic<bool> _stopping{ false };
    std::mutex _lock;
    std::map<connection_id, connection> _connections;
};

std::expected<std::unique_ptr<unix_transport>, error> unix_transport::listen(const path& location, transport_events& events)
{
    auto listening = unix_listener::listen(location);
    if (! listening) {
        return std::unexpected(std::move(listening).error());
    }
    auto watcher = poller::create();
    if (! watcher) {
        return std::unexpected(std::move(watcher).error());
    }
    if (auto added = watcher->add(listening->fd(), poller::interest::read); ! added) {
        return std::unexpected(std::move(added).error());
    }
    auto shared = std::make_shared<state>(events, std::move(*watcher), std::move(*listening));
    return std::unique_ptr<unix_transport>{ new unix_transport{ std::move(shared) } };
}

std::expected<std::unique_ptr<unix_transport>, error> unix_transport::connect(const path& location, transport_events& events)
{
    auto socket = unix_connect(location);
    if (! socket) {
        return std::unexpected(std::move(socket).error());
    }
    if (auto refused = refusal_of(socket->get())) {
        return std::unexpected(std::move(*refused));
    }
    auto watcher = poller::create();
    if (! watcher) {
        return std::unexpected(std::move(watcher).error());
    }
    auto shared = std::make_shared<state>(events, std::move(*watcher), std::nullopt);
    shared->adopt(std::move(*socket));
    return std::unique_ptr<unix_transport>{ new unix_transport{ std::move(shared) } };
}

unix_transport::unix_transport(std::shared_ptr<state> shared)
    : _state{ std::move(shared) }
    , _thread{ [shared = _state] {
        shared->run();
    } }
{
}

unix_transport::~unix_transport()
{
    _state->stop();
    if (_thread.get_id() == std::this_thread::get_id()) {
        _thread.detach();
    } else {
        _thread.join();
    }
}

std::expected<void, error> unix_transport::send(const connection_id id, const std::span<const std::byte> message)
{
    return _state->send(id, message);
}

void unix_transport::close(const connection_id id)
{
    _state->close(id);
}

}  // namespace dross
