#include "ipc/unix_transport.h"

#include "dross/ipc/error.h"
#include "ipc/accept_pause.h"
#include "ipc/frame.h"
#include "ipc/outbox.h"
#include "ipc/poller.h"
#include "ipc/unix_socket.h"
#include "thread/start_thread.h"

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dross {

namespace {

// One connection. The members of shared are touched only under the lock,
// the others only on the thread of the transport.
struct connection final {
    explicit connection(socket_handle accepted)
        : socket{ std::move(accepted) }
    {
    }

    struct shared_state final {
        outbox waiting;
        // Set when the connection is to be closed, with the reason.
        std::optional<error> ending;
    };

    socket_handle socket;
    frame_reader reader;
    // Whether the socket was added to the poller.
    bool added{ false };
    poller::interest watched{ poller::interest::read };
    bool announced{ false };
    shared_state shared;
};

// Whether the peer runs as the allowed user.
std::optional<error> refusal_of(const int fd, const uid_t allowed)
{
    const auto user = peer_uid(fd);
    if (! user) {
        return user.error();
    }
    if (*user != allowed) {
        return error{ ipc_errc::peer_not_same_user };
    }
    return std::nullopt;
}

}  // namespace

class unix_transport::state final {
public:
    state(transport_events& events, poller watcher, std::optional<unix_listener> listening, const uid_t allowed)
        : _allowed{ allowed }
        , _events{ events }
        , _poller{ std::move(watcher) }
        , _listening{ std::move(listening) }
        , _buffer(max_message_size)
    {
    }

    // Before the thread starts, or on it. Returns why the connection ends
    // at once, when it does.
    std::optional<error> adopt(socket_handle socket)
    {
        const int fd = socket.get();
        connection made{ std::move(socket) };
        made.shared.ending = refusal_of(fd, _allowed);
        if (! made.shared.ending) {
            if (auto added = _poller.add(fd, poller::interest::read); ! added) {
                made.shared.ending = std::move(added).error();
            } else {
                made.added = true;
            }
        }
        std::optional<error> ending = made.shared.ending;
        const connection_id id = _next_id++;
        _ids_by_fd[fd] = id;
        const std::lock_guard guard{ _lock };
        _connections.emplace(id, std::move(made));
        return ending;
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
            if ((found == _connections.end()) || found->second.shared.ending) {
                return std::unexpected(error{ std::errc::not_connected });
            }
            if (auto pushed = found->second.shared.waiting.push(std::move(*framed)); ! pushed) {
                found->second.shared.ending = pushed.error();
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
            if ((found == _connections.end()) || found->second.shared.ending) {
                return;
            }
            found->second.shared.ending = error{};
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
            const auto ready = _poller.wait(until_accepting());
            if (! ready) {
                end_all(ready.error());
                if (settle()) {
                    // Closed first, so the path is free to listen on again.
                    _listening.reset();
                    _events.stopped(ready.error());
                }
                return;
            }
            resume_accepting();
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
                if (each.shared.ending) {
                    if (each.added) {
                        _poller.remove(fd);
                    }
                    _ids_by_fd.erase(fd);
                    ended.emplace_back(it->first, std::move(*each.shared.ending));
                    it = _connections.erase(it);
                    continue;
                }
                if (! each.announced) {
                    each.announced = true;
                    fresh.push_back(it->first);
                }
                const auto wanted = each.shared.waiting.empty() ? poller::interest::read : poller::interest::read_write;
                if (wanted != each.watched) {
                    if (auto changed = _poller.change(fd, wanted); ! changed) {
                        each.shared.ending = std::move(changed).error();
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
            if (! accepted) {
                pause_accepting();
                return;
            }
            if (! *accepted) {
                return;
            }
            _accept_pause.accepted();
            adopt(std::move(**accepted));
        }
    }

    // Stops watching the listener for a while, since it stays ready while
    // the connection it failed to accept waits.
    void pause_accepting()
    {
        _poller.remove(_listening->fd());
        _accept_pause.pause(std::chrono::steady_clock::now());
    }

    // How long the wait may last before accepting resumes; none while
    // accepting.
    std::optional<std::chrono::milliseconds> until_accepting() const
    {
        return _accept_pause.remaining(std::chrono::steady_clock::now());
    }

    void resume_accepting()
    {
        const auto now = std::chrono::steady_clock::now();
        if (! _accept_pause.due(now)) {
            return;
        }
        if (auto added = _poller.add(_listening->fd(), poller::interest::read); ! added) {
            _accept_pause.pause(now);
            return;
        }
        _accept_pause.clear();
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
            if (! served->shared.ending) {
                if (auto flushed = served->shared.waiting.flush(served->socket.get()); ! flushed) {
                    served->shared.ending = std::move(flushed).error();
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
        if ((found == _connections.end()) || found->second.shared.ending) {
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
            end(from, from.reader.holds_partial() ? error{ ipc_errc::message_cut_short } : error{});
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
        if (! which.shared.ending) {
            which.shared.ending = std::move(reason);
        }
    }

    void end_all(const error& reason)
    {
        const std::lock_guard guard{ _lock };
        for (auto& [id, each] : _connections) {
            if (! each.shared.ending) {
                each.shared.ending = reason;
            }
        }
    }

    const uid_t _allowed;
    transport_events& _events;
    poller _poller;
    std::optional<unix_listener> _listening;
    accept_pause _accept_pause;
    std::vector<std::byte> _buffer;
    std::unordered_map<int, connection_id> _ids_by_fd;
    connection_id _next_id{ 1 };
    std::atomic<bool> _stopping{ false };
    std::mutex _lock;
    std::map<connection_id, connection> _connections;
};

std::expected<std::unique_ptr<unix_transport>, error> unix_transport::listen(const path& location, transport_events& events)
{
    return listen(location, events, ::geteuid());
}

std::expected<std::unique_ptr<unix_transport>, error> unix_transport::listen(const path& location, transport_events& events, const uid_t allowed)
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
    auto shared = std::make_shared<state>(events, std::move(*watcher), std::move(*listening), allowed);
    return std::unique_ptr<unix_transport>{ new unix_transport{ std::move(shared) } };
}

std::expected<std::unique_ptr<unix_transport>, error> unix_transport::connect(const path& location, transport_events& events)
{
    return connect(location, events, ::geteuid());
}

std::expected<std::unique_ptr<unix_transport>, error> unix_transport::connect(const path& location, transport_events& events, const uid_t allowed)
{
    auto socket = unix_connect(location);
    if (! socket) {
        return std::unexpected(std::move(socket).error());
    }
    auto watcher = poller::create();
    if (! watcher) {
        return std::unexpected(std::move(watcher).error());
    }
    auto shared = std::make_shared<state>(events, std::move(*watcher), std::nullopt, allowed);
    if (auto ended = shared->adopt(std::move(*socket))) {
        return std::unexpected(std::move(*ended));
    }
    return std::unique_ptr<unix_transport>{ new unix_transport{ std::move(shared) } };
}

unix_transport::unix_transport(std::shared_ptr<state> shared)
    : _state{ std::move(shared) }
{
}

std::expected<void, error> unix_transport::start()
{
    auto started = start_thread([shared = _state] {
        shared->run();
    });
    if (! started) {
        return std::unexpected(std::move(started).error());
    }
    _thread = std::move(*started);
    return {};
}

unix_transport::~unix_transport()
{
    _state->stop();
    if (! _thread.joinable()) {
        return;
    }
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
