#include "notification/connector.h"

#include "dross/ipc/message_channel.h"
#include "ipc/backoff.h"
#include "notification/part_calls.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace dross {

// What the connector's thread and the attempts share.
class connector::state final {
public:
    std::mutex lock;
    std::condition_variable wake;
    std::shared_ptr<ipc::message_channel> channel;
    std::shared_ptr<attempt> current;
    std::optional<error> closing;
    backoff waits;
    bool ended{ false };
    bool stopping{ false };
};

// The events of one try. It tells the layer above one call at a time, and
// holds what comes before the connector has the channel. Each call keeps it
// going, as the part may let it go inside the call.
class connector::attempt final : public ipc::message_channel_events, public std::enable_shared_from_this<attempt> {
public:
    attempt(std::weak_ptr<state> shared, std::weak_ptr<connector_events> above)
        : _state{ std::move(shared) }
        , _above{ std::move(above) }
    {
    }

    void received(std::vector<std::byte> message) override
    {
        const auto self = shared_from_this();
        const auto call = _calls.any("received");
        const std::lock_guard guard{ _telling };
        if (! _opened) {
            _early.push_back(std::move(message));
            return;
        }
        tell_received(std::move(message));
    }

    void closed(error reason) override
    {
        const auto self = shared_from_this();
        const auto call = _calls.last("closed");
        const std::lock_guard guard{ _telling };
        if (! _opened) {
            _early_end = std::move(reason);
            return;
        }
        finish(std::move(reason));
    }

    // Called on the connector's thread once it holds the channel.
    void open()
    {
        const std::lock_guard guard{ _telling };
        _opened = true;
        if (const auto shared = _state.lock()) {
            const std::lock_guard state_guard{ shared->lock };
            shared->waits.reset();
        }
        tell([](connector_events& above) {
            above.connected();
        });
        for (auto& message : _early) {
            tell_received(std::move(message));
        }
        _early.clear();
        if (_early_end) {
            finish(std::move(*_early_end));
        }
    }

    // Tells nothing more, and returns once a call under way has ended,
    // unless it is called from inside that call.
    void silence()
    {
        _silenced = true;
        if (_telling_thread.load() == std::this_thread::get_id()) {
            return;
        }
        const std::lock_guard guard{ _telling };
    }

private:
    void tell_received(std::vector<std::byte> message)
    {
        tell([&message](connector_events& above) {
            above.received(std::move(message));
        });
    }

    // Tells the end, then wakes the connector's thread for the next try.
    void finish(error reason)
    {
        const auto shared = _state.lock();
        if (shared) {
            const std::lock_guard guard{ shared->lock };
            if (shared->closing) {
                reason = std::move(*shared->closing);
                shared->closing.reset();
            }
        }
        tell([&reason](connector_events& above) {
            above.disconnected(std::move(reason));
        });
        if (shared) {
            const std::lock_guard guard{ shared->lock };
            shared->ended = true;
            shared->wake.notify_all();
        }
    }

    // Letting above go may destroy the connector, which then finds this
    // thread still telling and does not wait for it.
    template <typename F>
    void tell(F&& call)
    {
        if (_silenced) {
            return;
        }
        auto above = _above.lock();
        if (! above) {
            return;
        }
        _telling_thread = std::this_thread::get_id();
        call(*above);
        above.reset();
        _telling_thread = std::thread::id{};
    }

    const std::weak_ptr<state> _state;
    const std::weak_ptr<connector_events> _above;
    part_calls _calls{ "message_channel_events" };
    std::mutex _telling;
    std::atomic<std::thread::id> _telling_thread;
    std::atomic<bool> _silenced{ false };
    bool _opened{ false };
    std::vector<std::vector<std::byte>> _early;
    std::optional<error> _early_end;
};

connector::connector(std::shared_ptr<ipc::message_connector> part, std::weak_ptr<connector_events> above)
    : _part{ std::move(part) }
    , _above{ std::move(above) }
    , _state{ std::make_shared<state>() }
{
}

connector::~connector()
{
    std::shared_ptr<ipc::message_channel> channel;
    std::shared_ptr<attempt> current;
    {
        const std::lock_guard guard{ _state->lock };
        _state->stopping = true;
        channel = std::move(_state->channel);
        current = std::move(_state->current);
        _state->wake.notify_all();
    }
    if (current) {
        current->silence();
    }
    if (channel) {
        channel->close();
    }
    if (! _thread.joinable()) {
        return;
    }
    // Let go when this runs on the connector's own thread, inside
    // connected(): the thread touches only what it shares from then on.
    if (_thread.get_id() == std::this_thread::get_id()) {
        _thread.detach();
    } else {
        _thread.join();
    }
}

void connector::start()
{
    _thread = std::thread{ [shared = _state, part = _part, above = _above] {
        run(shared, part, above);
    } };
}

void connector::send(const std::span<const std::byte> message)
{
    std::shared_ptr<ipc::message_channel> channel;
    {
        const std::lock_guard guard{ _state->lock };
        channel = _state->channel;
    }
    if (channel) {
        // A message larger than the channel carries is not sent and the
        // connection stays; one that overflows the queue closes it, and it
        // is then made again.
        (void)channel->send(message);
    }
}

void connector::close(error reason)
{
    std::shared_ptr<ipc::message_channel> channel;
    {
        const std::lock_guard guard{ _state->lock };
        if (! _state->channel) {
            return;
        }
        _state->closing = std::move(reason);
        channel = _state->channel;
    }
    channel->close();
}

void connector::run(const std::shared_ptr<state>& shared, const std::shared_ptr<ipc::message_connector>& part, const std::weak_ptr<connector_events>& above)
{
    std::unique_lock guard{ shared->lock };
    while (! shared->stopping) {
        guard.unlock();
        auto events = std::make_shared<attempt>(shared, above);
        auto made = part->connect(events);
        guard.lock();
        if (shared->stopping) {
            if (made) {
                guard.unlock();
                (*made)->close();
            }
            return;
        }
        if (made) {
            shared->channel = std::move(*made);
            shared->current = events;
            shared->ended = false;
            guard.unlock();
            events->open();
            guard.lock();
            shared->wake.wait(guard, [&shared] {
                return shared->stopping || shared->ended;
            });
            if (shared->stopping) {
                return;
            }
            shared->channel.reset();
            shared->current.reset();
        }
        if (shared->wake.wait_for(guard, shared->waits.next(), [&shared] {
            return shared->stopping;
        })) {
            return;
        }
    }
}

}  // namespace dross
