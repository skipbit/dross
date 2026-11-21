#include "notification/publisher.h"

#include "dross/ipc/message_channel.h"
#include "notification/channel_calls.h"
#include "notification/subscriber_table.h"
#include "notification/wire.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace dross {

// What the part calls. It is shared with the part, which holds it until
// it is destroyed.
class publisher::core final : public ipc::message_acceptor_events, public std::enable_shared_from_this<core> {
public:
    explicit core(listen_function listen)
        : _listen{ std::move(listen) }
    {
    }

    std::expected<void, error> start()
    {
        auto made = _listen(shared_from_this());
        if (! made) {
            return std::unexpected(std::move(made).error());
        }
        const std::lock_guard guard{ _lock };
        _acceptor = std::move(*made);
        return {};
    }

    // Destroys what listens, which waits for its calls, and lets the
    // channels go.
    void stop()
    {
        std::unique_ptr<ipc::message_acceptor> ending;
        {
            const std::lock_guard guard{ _lock };
            _stopping = true;
            ending = std::move(_acceptor);
        }
        ending.reset();
        const std::lock_guard guard{ _lock };
        _subscribers.clear();
    }

    bool wanted(const std::string_view name) const
    {
        const std::lock_guard guard{ _lock };
        return _subscribers.wanted(name);
    }

    void send(const std::string_view name, const value& content)
    {
        std::vector<subscriber_table::channel> receivers;
        {
            const std::lock_guard guard{ _lock };
            receivers = _subscribers.receivers(name);
        }
        if (receivers.empty()) {
            return;
        }
        const auto bytes = wire::encode(wire::notice{ std::string{ name }, content });
        // A message larger than the channel carries is not sent and the
        // connection stays; a channel whose queue overflows is closed by
        // the part, and its listener connects again.
        for (const auto& channel : receivers) {
            (void)channel->send(bytes);
        }
    }

    std::shared_ptr<ipc::message_channel_events> accepted(std::shared_ptr<ipc::message_channel> channel) override
    {
        {
            const std::lock_guard guard{ _lock };
            _subscribers.join(channel);
        }
        return std::make_shared<subscriber_events>(weak_from_this(), channel);
    }

    void stopped(error) override
    {
        // Called from the stopped acceptor, which lets its calls go once
        // this returns.
        {
            const std::lock_guard guard{ _lock };
            if (_stopping) {
                return;
            }
        }
        auto again = _listen(shared_from_this());
        std::unique_ptr<ipc::message_acceptor> replaced;
        const std::lock_guard guard{ _lock };
        replaced = std::move(_acceptor);
        if (! again || _stopping) {
            return;
        }
        _acceptor = std::move(*again);
    }

    void received(const subscriber_table::channel& from, const std::vector<std::byte>& message)
    {
        auto read = wire::decode(message);
        if (read && ! std::holds_alternative<wire::notice>(*read)) {
            const std::lock_guard guard{ _lock };
            if (const auto* asked = std::get_if<wire::subscribe>(&*read)) {
                _subscribers.add(from, asked->name);
            } else {
                _subscribers.remove(from, std::get<wire::unsubscribe>(*read).name);
            }
            return;
        }
        // A listener sends nothing else, so the peer is not one.
        from->close();
    }

    void closed(const subscriber_table::channel& from)
    {
        const std::lock_guard guard{ _lock };
        _subscribers.leave(from);
    }

private:
    // What one accepted channel tells. It holds the core and the channel
    // without keeping them going.
    class subscriber_events final : public ipc::message_channel_events {
    public:
        subscriber_events(std::weak_ptr<core> above, std::weak_ptr<ipc::message_channel> channel)
            : _above{ std::move(above) }
            , _channel{ std::move(channel) }
        {
        }

        void received(std::vector<std::byte> message) override
        {
            const auto call = _calls.received();
            const auto above = _above.lock();
            const auto channel = _channel.lock();
            if (above && channel) {
                above->received(channel, message);
            }
        }

        void closed(error) override
        {
            const auto call = _calls.closed();
            const auto above = _above.lock();
            const auto channel = _channel.lock();
            if (above && channel) {
                above->closed(channel);
            }
        }

    private:
        const std::weak_ptr<core> _above;
        const std::weak_ptr<ipc::message_channel> _channel;
        channel_calls _calls;
    };

    const listen_function _listen;
    mutable std::mutex _lock;
    std::unique_ptr<ipc::message_acceptor> _acceptor;
    subscriber_table _subscribers;
    // Set by stop(), so that stopped() does not listen again for nothing to
    // stop.
    bool _stopping{ false };
};

std::expected<std::unique_ptr<publisher>, error> publisher::listen(listen_function listen)
{
    auto shared = std::make_shared<core>(std::move(listen));
    if (auto started = shared->start(); ! started) {
        return std::unexpected(std::move(started).error());
    }
    return std::unique_ptr<publisher>{ new publisher{ std::move(shared) } };
}

publisher::publisher(std::shared_ptr<core> shared)
    : _core{ std::move(shared) }
{
}

publisher::~publisher()
{
    _core->stop();
}

bool publisher::wanted(const std::string_view name) const
{
    return _core->wanted(name);
}

void publisher::send(const std::string_view name, const value& content)
{
    _core->send(name, content);
}

}  // namespace dross
