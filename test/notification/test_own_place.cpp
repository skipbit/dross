#include "dross/ipc/message_acceptor.h"
#include "dross/ipc/message_channel.h"
#include "dross/ipc/message_connector.h"
#include "dross/notification/broadcaster.h"
#include "dross/notification/delivery.h"
#include "dross/notification/listener.h"
#include "dross/notification/topic.h"
#include "dross/type/value.h"
#include "notification/wire.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;
using bytes = std::vector<std::byte>;

inline constexpr dross::topic<std::int64_t> kCount{ "com.example.test.count" };

// Waits on a condition that other threads change, under one lock.
class waitable {
public:
    bool wait_for(const std::function<bool()>& done)
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, 5s, done);
    }

protected:
    template <typename F>
    void change(F&& how)
    {
        const std::lock_guard guard{ _lock };
        how();
        _changed.notify_all();
    }

private:
    std::mutex _lock;
    std::condition_variable _changed;
};

// A channel that keeps what is sent on it.
class kept_channel final : public dross::ipc::message_channel, public waitable {
public:
    std::expected<void, dross::error> send(std::span<const std::byte> message) override
    {
        change([&] {
            _sent.emplace_back(message.begin(), message.end());
        });
        return {};
    }

    void close() override
    {
    }

    // Read inside a condition given to wait_for(), which holds the lock.
    const std::vector<bytes>& sent_held() const
    {
        return _sent;
    }

private:
    std::vector<bytes> _sent;
};

// Holds the events of a listen() until it is destroyed, as a part does.
class holding_acceptor final : public dross::ipc::message_acceptor {
public:
    explicit holding_acceptor(std::shared_ptr<dross::ipc::message_acceptor_events> events)
        : _events{ std::move(events) }
    {
    }

private:
    const std::shared_ptr<dross::ipc::message_acceptor_events> _events;
};

// What a place of the test's own keeps, shared by every copy of it.
struct listened final {
    std::weak_ptr<dross::ipc::message_acceptor_events> events;
    int listens{ 0 };
};

// A place of the test's own to publish at, copied as the concept asks.
class own_listenable_place {
public:
    explicit own_listenable_place(std::shared_ptr<listened> state)
        : _state{ std::move(state) }
    {
    }

    std::expected<std::unique_ptr<dross::ipc::message_acceptor>, dross::error> listen(std::shared_ptr<dross::ipc::message_acceptor_events> events) const
    {
        _state->events = events;
        ++_state->listens;
        return std::make_unique<holding_acceptor>(std::move(events));
    }

private:
    std::shared_ptr<listened> _state;
};

// Gives each try the one channel, and keeps the events of the last.
class own_connector final : public dross::ipc::message_connector, public waitable {
public:
    std::expected<std::shared_ptr<dross::ipc::message_channel>, dross::error> connect(std::shared_ptr<dross::ipc::message_channel_events> events) override
    {
        change([&] {
            _events = std::move(events);
        });
        return channel;
    }

    std::shared_ptr<dross::ipc::message_channel_events> events()
    {
        std::shared_ptr<dross::ipc::message_channel_events> found;
        change([&] {
            found = _events;
        });
        return found;
    }

    const std::shared_ptr<kept_channel> channel = std::make_shared<kept_channel>();

private:
    std::shared_ptr<dross::ipc::message_channel_events> _events;
};

// A place of the test's own to connect to. It cannot be copied, which the
// concept does not ask for.
class own_connectable_place {
public:
    explicit own_connectable_place(std::shared_ptr<own_connector> part)
        : _part{ std::move(part) }
    {
    }

    own_connectable_place(const own_connectable_place& other) = delete;
    own_connectable_place& operator=(const own_connectable_place& other) = delete;

    std::shared_ptr<dross::ipc::message_connector> connector() const
    {
        return _part;
    }

private:
    std::shared_ptr<own_connector> _part;
};

class heard final : public waitable {
public:
    void add(const std::int64_t value)
    {
        change([&] {
            _value = value;
        });
    }

    // Read inside a condition given to wait_for(), which holds the lock.
    std::optional<std::int64_t> value_held() const
    {
        return _value;
    }

private:
    std::optional<std::int64_t> _value;
};

template <typename P>
concept publishable_at = requires(const P& place) { dross::broadcaster::create(place); };

template <typename P>
concept listenable_at = requires(const P& place) { dross::listener::create(place); };

static_assert(publishable_at<own_listenable_place>);
static_assert(! publishable_at<own_connectable_place>);
static_assert(! publishable_at<int>);
static_assert(listenable_at<own_connectable_place>);
static_assert(! listenable_at<own_listenable_place>);
static_assert(! listenable_at<int>);

// A place that gives no connector.
class connectorless_place {
public:
    std::shared_ptr<dross::ipc::message_connector> connector() const
    {
        return nullptr;
    }
};

TEST(own_place_test, a_place_that_gives_no_connector_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH((void)dross::listener::create(connectorless_place{}), "listener::create: the place gave no connector");
}

TEST(own_place_test, a_broadcaster_published_at_a_place_of_its_own_sends_there)
{
    const auto state = std::make_shared<listened>();
    auto news = dross::broadcaster::create(own_listenable_place{ state });
    ASSERT_TRUE(news.has_value());
    ASSERT_EQ(state->listens, 1);

    auto channel = std::make_shared<kept_channel>();
    auto events = state->events.lock()->accepted(channel);
    events->received(dross::wire::encode(dross::wire::subscribe{ std::string{ kCount.name() } }));
    news->notify(kCount, 42);

    const auto sent = dross::wire::encode(dross::wire::notice{ std::string{ kCount.name() }, dross::value{ std::int64_t{ 42 } } });
    EXPECT_TRUE(channel->wait_for([&] {
        return channel->sent_held() == std::vector<bytes>{ sent };
    }));
}

TEST(own_place_test, the_place_is_copied_to_listen_there_again)
{
    const auto state = std::make_shared<listened>();
    std::optional<dross::broadcaster> news;
    {
        const own_listenable_place place{ state };
        auto made = dross::broadcaster::create(place);
        ASSERT_TRUE(made.has_value());
        news = *made;
    }
    state->events.lock()->stopped(dross::error{ std::errc::io_error });
    EXPECT_EQ(state->listens, 2);
}

TEST(own_place_test, a_listener_hears_through_a_place_of_its_own)
{
    const auto part = std::make_shared<own_connector>();
    auto ears = dross::listener::create(own_connectable_place{ part }).value();
    heard got;
    auto subscribed = ears.subscribe(kCount, [&got](const std::int64_t& value) {
        got.add(value);
    }, dross::delivery::direct());
    ASSERT_TRUE(subscribed.has_value());

    const auto asked = dross::wire::encode(dross::wire::subscribe{ std::string{ kCount.name() } });
    ASSERT_TRUE(part->channel->wait_for([&] {
        return part->channel->sent_held() == std::vector<bytes>{ asked };
    }));
    part->events()->received(dross::wire::encode(dross::wire::notice{ std::string{ kCount.name() }, dross::value{ std::int64_t{ 7 } } }));
    EXPECT_TRUE(got.wait_for([&] {
        return got.value_held() == std::int64_t{ 7 };
    }));
}

}  // namespace
