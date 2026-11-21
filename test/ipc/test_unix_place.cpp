#include "dross/ipc/error.h"
#include "dross/ipc/message_place.h"
#include "dross/ipc/unix_socket.h"
#include "ipc/socket_directory.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;
using bytes = std::vector<std::byte>;

constexpr auto kPatience = 5s;

bytes bytes_of(std::initializer_list<int> values)
{
    bytes out;
    for (const int v : values) {
        out.push_back(static_cast<std::byte>(v));
    }
    return out;
}

// Records what happens on one connection.
class recorder final : public dross::ipc::message_channel_events {
public:
    void received(bytes message) override
    {
        const std::lock_guard guard{ _lock };
        _messages.push_back(std::move(message));
        _changed.notify_all();
    }

    void closed(dross::error reason) override
    {
        const std::lock_guard guard{ _lock };
        ++_closes;
        _reason = std::move(reason);
        _changed.notify_all();
    }

    bool wait_for_messages(std::size_t count)
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, kPatience, [&] {
            return _messages.size() >= count;
        });
    }

    bool wait_for_close()
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, kPatience, [&] {
            return _closes > 0;
        });
    }

    std::vector<bytes> messages()
    {
        const std::lock_guard guard{ _lock };
        return _messages;
    }

    int closes()
    {
        const std::lock_guard guard{ _lock };
        return _closes;
    }

    std::optional<dross::error> reason()
    {
        const std::lock_guard guard{ _lock };
        return _reason;
    }

private:
    std::mutex _lock;
    std::condition_variable _changed;
    std::vector<bytes> _messages;
    int _closes{ 0 };
    std::optional<dross::error> _reason;
};

// Records what is accepted, giving each connection a recorder.
class acceptance final : public dross::ipc::message_acceptor_events {
public:
    std::shared_ptr<dross::ipc::message_channel_events> accepted(std::shared_ptr<dross::ipc::message_channel> channel) override
    {
        auto events = std::make_shared<recorder>();
        if (on_accept) {
            on_accept();
        }
        const std::lock_guard guard{ _lock };
        _channels.push_back(std::move(channel));
        _events.push_back(events);
        _changed.notify_all();
        return events;
    }

    void stopped(dross::error) override
    {
    }

    bool wait_for_accepted(std::size_t count)
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, kPatience, [&] {
            return _channels.size() >= count;
        });
    }

    std::shared_ptr<dross::ipc::message_channel> channel(std::size_t at)
    {
        const std::lock_guard guard{ _lock };
        return _channels.at(at);
    }

    std::shared_ptr<recorder> events(std::size_t at)
    {
        const std::lock_guard guard{ _lock };
        return _events.at(at);
    }

    std::function<void()> on_accept;

private:
    std::mutex _lock;
    std::condition_variable _changed;
    std::vector<std::shared_ptr<dross::ipc::message_channel>> _channels;
    std::vector<std::shared_ptr<recorder>> _events;
};

class unix_place_test : public dross_test::socket_directory_test {
protected:
    dross::ipc::unix_socket place() const
    {
        return dross::ipc::unix_socket{ dross::path{ at("place.sock") } };
    }
};

static_assert(dross::ipc::message_listenable_place_type<dross::ipc::unix_socket>);
static_assert(dross::ipc::message_connectable_place_type<dross::ipc::unix_socket>);

TEST_F(unix_place_test, messages_cross_both_ways_in_order)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = place().listen(above);
    ASSERT_TRUE(acceptor.has_value());

    auto mine = std::make_shared<recorder>();
    auto channel = place().connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE(above->wait_for_accepted(1));

    ASSERT_TRUE((*channel)->send(bytes_of({ 1 })).has_value());
    ASSERT_TRUE((*channel)->send(bytes_of({ 2, 3 })).has_value());
    ASSERT_TRUE(above->events(0)->wait_for_messages(2));
    EXPECT_EQ(above->events(0)->messages(), (std::vector<bytes>{ bytes_of({ 1 }), bytes_of({ 2, 3 }) }));

    ASSERT_TRUE(above->channel(0)->send(bytes_of({ 4 })).has_value());
    ASSERT_TRUE(mine->wait_for_messages(1));
    EXPECT_EQ(mine->messages(), std::vector<bytes>{ bytes_of({ 4 }) });
}

TEST_F(unix_place_test, connecting_where_nothing_listens_fails)
{
    auto mine = std::make_shared<recorder>();
    const auto channel = place().connector()->connect(mine);
    ASSERT_FALSE(channel.has_value());
    EXPECT_EQ(channel.error(), dross::error(std::errc::no_such_file_or_directory));
    EXPECT_EQ(mine->closes(), 0);
}

TEST_F(unix_place_test, listening_where_another_listens_fails)
{
    auto above = std::make_shared<acceptance>();
    auto first = place().listen(above);
    ASSERT_TRUE(first.has_value());
    const auto second = place().listen(std::make_shared<acceptance>());
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), dross::error(std::errc::address_in_use));
}

TEST_F(unix_place_test, closing_from_the_connecting_side_ends_both_once)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = place().listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto mine = std::make_shared<recorder>();
    auto channel = place().connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE(above->wait_for_accepted(1));

    (*channel)->close();
    ASSERT_TRUE(mine->wait_for_close());
    ASSERT_TRUE(above->events(0)->wait_for_close());
    EXPECT_EQ(mine->reason(), dross::error{});
    EXPECT_EQ(above->events(0)->reason(), dross::error{});

    (*channel)->close();
    const auto refused = (*channel)->send(bytes_of({ 1 }));
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), dross::error(std::errc::not_connected));
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(mine->closes(), 1);
    EXPECT_EQ(above->events(0)->closes(), 1);
}

TEST_F(unix_place_test, closing_from_the_accepting_side_ends_both)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = place().listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto mine = std::make_shared<recorder>();
    auto channel = place().connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE(above->wait_for_accepted(1));

    above->channel(0)->close();
    ASSERT_TRUE(above->events(0)->wait_for_close());
    ASSERT_TRUE(mine->wait_for_close());
    const auto refused = above->channel(0)->send(bytes_of({ 1 }));
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), dross::error(std::errc::not_connected));
}

TEST_F(unix_place_test, letting_the_connecting_channel_go_keeps_the_connection)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = place().listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto mine = std::make_shared<recorder>();
    {
        auto channel = place().connector()->connect(mine);
        ASSERT_TRUE(channel.has_value());
    }
    ASSERT_TRUE(above->wait_for_accepted(1));

    ASSERT_TRUE(above->channel(0)->send(bytes_of({ 5 })).has_value());
    ASSERT_TRUE(mine->wait_for_messages(1));

    above->channel(0)->close();
    ASSERT_TRUE(mine->wait_for_close());
}

TEST_F(unix_place_test, destroying_the_acceptor_ends_the_peers_without_closed_on_its_own_side)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = place().listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto mine = std::make_shared<recorder>();
    auto channel = place().connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE(above->wait_for_accepted(1));

    acceptor->reset();
    ASSERT_TRUE(mine->wait_for_close());
    EXPECT_EQ(above->events(0)->closes(), 0);
    const auto refused = above->channel(0)->send(bytes_of({ 1 }));
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), dross::error(std::errc::not_connected));
}

TEST_F(unix_place_test, the_acceptor_can_be_destroyed_from_inside_accepted)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = place().listen(above);
    ASSERT_TRUE(acceptor.has_value());
    std::mutex lock;
    std::unique_ptr<dross::ipc::message_acceptor> held = std::move(*acceptor);
    above->on_accept = [&] {
        const std::lock_guard guard{ lock };
        held.reset();
    };

    auto mine = std::make_shared<recorder>();
    auto channel = place().connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE(mine->wait_for_close());
    const std::lock_guard guard{ lock };
    EXPECT_EQ(held, nullptr);
}

TEST_F(unix_place_test, the_events_of_a_connection_accepted_while_the_acceptor_goes_are_let_go)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = place().listen(above);
    ASSERT_TRUE(acceptor.has_value());
    std::mutex lock;
    std::unique_ptr<dross::ipc::message_acceptor> held = std::move(*acceptor);
    above->on_accept = [&] {
        const std::lock_guard guard{ lock };
        held.reset();
    };

    auto mine = std::make_shared<recorder>();
    auto channel = place().connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE(above->wait_for_accepted(1));

    // Only the test's own record of what was accepted holds them once the
    // part has let them go.
    const std::weak_ptr<recorder> events = above->events(0);
    const auto until = std::chrono::steady_clock::now() + kPatience;
    while (events.use_count() > 1 && std::chrono::steady_clock::now() < until) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(events.use_count(), 1);
}

// Gives nothing to tell about each connection it accepts.
class giving_null final : public dross::ipc::message_acceptor_events {
public:
    std::shared_ptr<dross::ipc::message_channel_events> accepted(std::shared_ptr<dross::ipc::message_channel>) override
    {
        return nullptr;
    }

    void stopped(dross::error) override
    {
    }
};

TEST_F(unix_place_test, accepted_that_returns_null_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH(
        {
            auto acceptor = place().listen(std::make_shared<giving_null>());
            auto mine = std::make_shared<recorder>();
            auto channel = place().connector()->connect(mine);
            std::this_thread::sleep_for(kPatience);
        },
        "message_acceptor_events::accepted: returned null");
}

TEST_F(unix_place_test, a_message_over_the_largest_fails)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = place().listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto channel = place().connector()->connect(std::make_shared<recorder>());
    ASSERT_TRUE(channel.has_value());

    const bytes over(64 * 1024 + 1, std::byte{ 1 });
    const auto refused = (*channel)->send(over);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), dross::ipc_errc::message_too_large);
}

}  // namespace
