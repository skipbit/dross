#include "dross/ipc/unix_socket.h"
#include "ipc/socket_directory.h"
#include "ipc/unix_transport.h"
#include "notification/connector.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

class connector_test : public dross_test::socket_directory_test {
protected:
    // What connects to the socket file name in the test's directory.
    std::shared_ptr<dross::ipc::message_connector> place(const std::string& name) const
    {
        return dross::ipc::unix_socket{ dross::path{ at(name) } }.connector();
    }
};

// Waits on a condition that events change, under one lock.
class waitable {
public:
    bool wait_for(const std::function<bool()>& done, const std::chrono::milliseconds limit = 5s)
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, limit, done);
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

// Stands in for the layer above the connector.
class above final : public dross::connector_events, public waitable {
public:
    void connected() override
    {
        change([this] {
            ++_connections;
        });
    }

    void received(std::vector<std::byte> message) override
    {
        change([this, &message] {
            _messages.push_back(std::move(message));
        });
    }

    void disconnected(dross::error reason) override
    {
        change([this, &reason] {
            _reasons.push_back(std::move(reason));
        });
    }

    // Read inside a condition given to wait_for(), which holds the lock.
    int connections_held() const
    {
        return _connections;
    }

    const std::vector<std::vector<std::byte>>& messages_held() const
    {
        return _messages;
    }

    const std::vector<dross::error>& reasons_held() const
    {
        return _reasons;
    }

private:
    int _connections{ 0 };
    std::vector<std::vector<std::byte>> _messages;
    std::vector<dross::error> _reasons;
};

// Stands in for what listens: a transport at a path that keeps what it is
// sent.
class peer final : public dross::transport_events, public waitable {
public:
    explicit peer(const std::string& where)
    {
        auto made = dross::unix_transport::listen(dross::path{ where }, *this);
        EXPECT_TRUE(made.has_value());
        if (made) {
            _transport = std::move(*made);
            _transport->start();
        }
    }

    ~peer() override
    {
        _transport.reset();
    }

    bool joined()
    {
        return wait_for([this] {
            return _id.has_value();
        });
    }

    void send(const std::vector<std::byte>& message)
    {
        ASSERT_TRUE(joined());
        ASSERT_TRUE(_transport->send(*_id, message).has_value());
    }

    void close()
    {
        ASSERT_TRUE(joined());
        _transport->close(*_id);
    }

    const std::vector<std::vector<std::byte>>& messages_held() const
    {
        return _messages;
    }

    void connected(const dross::connection_id id) override
    {
        change([this, id] {
            _id = id;
        });
    }

    void received(dross::connection_id, std::vector<std::byte> message) override
    {
        change([this, &message] {
            _messages.push_back(std::move(message));
        });
    }

    void closed(dross::connection_id, dross::error) override
    {
        change([this] {
            _id.reset();
        });
    }

    void stopped(dross::error) override
    {
    }

private:
    std::unique_ptr<dross::unix_transport> _transport;
    std::optional<dross::connection_id> _id;
    std::vector<std::vector<std::byte>> _messages;
};

std::vector<std::byte> bytes(const std::string& text)
{
    std::vector<std::byte> made;
    for (const char each : text) {
        made.push_back(static_cast<std::byte>(each));
    }
    return made;
}

TEST_F(connector_test, connects_to_a_peer_that_listens_after_it_started)
{
    auto events = std::make_shared<above>();
    dross::connector link{ place("s"), events };
    link.start();

    EXPECT_FALSE(events->wait_for([&] {
        return events->connections_held() > 0;
    }, 300ms));
    EXPECT_TRUE(events->reasons_held().empty());

    peer there{ at("s") };
    EXPECT_TRUE(events->wait_for([&] {
        return events->connections_held() == 1;
    }));
}

TEST_F(connector_test, carries_messages_both_ways)
{
    peer there{ at("s") };
    auto events = std::make_shared<above>();
    dross::connector link{ place("s"), events };
    link.start();
    ASSERT_TRUE(events->wait_for([&] {
        return events->connections_held() == 1;
    }));

    link.send(bytes("up"));
    there.send(bytes("down"));

    EXPECT_TRUE(there.wait_for([&] {
        return there.messages_held() == std::vector<std::vector<std::byte>>{ bytes("up") };
    }));
    EXPECT_TRUE(events->wait_for([&] {
        return events->messages_held() == std::vector<std::vector<std::byte>>{ bytes("down") };
    }));
}

TEST_F(connector_test, drops_what_is_sent_while_not_connected)
{
    auto events = std::make_shared<above>();
    dross::connector link{ place("s"), events };
    link.send(bytes("early"));
    link.start();

    peer there{ at("s") };
    ASSERT_TRUE(events->wait_for([&] {
        return events->connections_held() == 1;
    }));
    link.send(bytes("late"));

    EXPECT_TRUE(there.wait_for([&] {
        return ! there.messages_held().empty();
    }));
    EXPECT_EQ(there.messages_held(), std::vector<std::vector<std::byte>>{ bytes("late") });
}

TEST_F(connector_test, connects_again_once_the_peer_has_gone_and_come_back)
{
    auto events = std::make_shared<above>();
    dross::connector link{ place("s"), events };
    {
        peer there{ at("s") };
        link.start();
        ASSERT_TRUE(events->wait_for([&] {
            return events->connections_held() == 1;
        }));
    }
    ASSERT_TRUE(events->wait_for([&] {
        return events->reasons_held().size() == 1;
    }));

    peer back{ at("s") };
    EXPECT_TRUE(events->wait_for([&] {
        return events->connections_held() == 2;
    }));
}

TEST_F(connector_test, reports_a_peer_that_ends_the_connection_in_order_with_no_reason)
{
    peer there{ at("s") };
    auto events = std::make_shared<above>();
    dross::connector link{ place("s"), events };
    link.start();
    ASSERT_TRUE(events->wait_for([&] {
        return events->connections_held() == 1;
    }));

    there.close();

    ASSERT_TRUE(events->wait_for([&] {
        return events->reasons_held().size() == 1;
    }));
    EXPECT_FALSE(events->reasons_held().front());
    EXPECT_TRUE(events->wait_for([&] {
        return events->connections_held() == 2;
    }));
}

TEST_F(connector_test, reports_the_reason_given_to_close_and_connects_again)
{
    peer there{ at("s") };
    auto events = std::make_shared<above>();
    dross::connector link{ place("s"), events };
    link.start();
    ASSERT_TRUE(events->wait_for([&] {
        return events->connections_held() == 1;
    }));

    link.close(dross::error{ std::errc::invalid_argument });

    ASSERT_TRUE(events->wait_for([&] {
        return events->reasons_held().size() == 1;
    }));
    EXPECT_EQ(events->reasons_held().front(), dross::error{ std::errc::invalid_argument });
    EXPECT_TRUE(events->wait_for([&] {
        return events->connections_held() == 2;
    }));
}

// Takes longer to hear that a connection ended than the first wait before
// connecting again, and notes whether a connection was made meanwhile.
class slow_to_hear_the_end final : public dross::connector_events, public waitable {
public:
    void connected() override
    {
        change([this] {
            ++_connections;
        });
    }

    void received(std::vector<std::byte>) override
    {
    }

    void disconnected(dross::error) override
    {
        int before = 0;
        change([this, &before] {
            before = _connections;
        });
        std::this_thread::sleep_for(300ms);
        change([this, before] {
            _overlapped = _overlapped || (_connections != before);
            ++_ends;
        });
    }

    // Read inside a condition given to wait_for(), which holds the lock.
    int connections_held() const
    {
        return _connections;
    }

    int ends_held() const
    {
        return _ends;
    }

    bool overlapped_held() const
    {
        return _overlapped;
    }

private:
    int _connections{ 0 };
    int _ends{ 0 };
    bool _overlapped{ false };
};

TEST_F(connector_test, connects_again_only_once_the_end_has_been_reported)
{
    peer there{ at("s") };
    auto events = std::make_shared<slow_to_hear_the_end>();
    dross::connector link{ place("s"), events };
    link.start();
    ASSERT_TRUE(events->wait_for([&] {
        return events->connections_held() == 1;
    }));

    there.close();

    EXPECT_TRUE(events->wait_for([&] {
        return (events->connections_held() == 2) && (events->ends_held() == 1) && ! events->overlapped_held();
    }));
}

// Lets the connector go from inside its own event, as a listener whose last
// handle goes inside a subscriber's function does.
class letting_go final : public dross::connector_events, public waitable {
public:
    void hold(std::unique_ptr<dross::connector> link)
    {
        change([this, &link] {
            _link = std::move(link);
        });
    }

    bool let_go()
    {
        return wait_for([this] {
            return _gone;
        });
    }

    void connected() override
    {
        // Waits for the connector, which is handed over once started.
        EXPECT_TRUE(wait_for([this] {
            return _link != nullptr;
        }));
        std::unique_ptr<dross::connector> ending;
        change([this, &ending] {
            ending = std::move(_link);
        });
        ending.reset();
        change([this] {
            _gone = true;
        });
    }

    void received(std::vector<std::byte>) override
    {
    }

    void disconnected(dross::error) override
    {
    }

private:
    std::unique_ptr<dross::connector> _link;
    bool _gone{ false };
};

TEST_F(connector_test, may_be_let_go_from_inside_its_own_event)
{
    peer there{ at("s") };
    auto events = std::make_shared<letting_go>();
    auto link = std::make_unique<dross::connector>(place("s"), events);
    link->start();
    events->hold(std::move(link));

    EXPECT_TRUE(events->let_go());
}

TEST_F(connector_test, calls_nothing_once_the_layer_above_has_gone)
{
    peer there{ at("s") };
    auto events = std::make_shared<above>();
    dross::connector link{ place("s"), std::weak_ptr<above>{ events } };
    events.reset();
    link.start();

    EXPECT_TRUE(there.joined());
}

// A part whose tries the test scripts.
class scripted_part final : public dross::ipc::message_connector, public waitable {
public:
    using script = std::function<std::expected<std::shared_ptr<dross::ipc::message_channel>, dross::error>(int, const std::shared_ptr<dross::ipc::message_channel_events>&)>;

    explicit scripted_part(script tries)
        : _script{ std::move(tries) }
    {
    }

    std::expected<std::shared_ptr<dross::ipc::message_channel>, dross::error> connect(std::shared_ptr<dross::ipc::message_channel_events> events) override
    {
        int now = 0;
        change([this, &now, &events] {
            now = ++_tries;
            _events = events;
        });
        return _script(now, events);
    }

    // Read inside a condition given to wait_for(), which holds the lock.
    int tries_held() const
    {
        return _tries;
    }

    std::shared_ptr<dross::ipc::message_channel_events> last_events()
    {
        std::shared_ptr<dross::ipc::message_channel_events> found;
        change([this, &found] {
            found = _events;
        });
        return found;
    }

private:
    const script _script;
    int _tries{ 0 };
    std::shared_ptr<dross::ipc::message_channel_events> _events;
};

class counted_channel final : public dross::ipc::message_channel {
public:
    std::expected<void, dross::error> send(std::span<const std::byte>) override
    {
        return {};
    }

    void close() override
    {
        ++closes;
    }

    std::atomic<int> closes{ 0 };
};

// Keeps what the connector tells, in order.
class told final : public dross::connector_events, public waitable {
public:
    void connected() override
    {
        change([this] {
            _log.push_back("connected");
        });
    }

    void received(std::vector<std::byte> message) override
    {
        change([this, &message] {
            _log.push_back("received " + std::to_string(static_cast<int>(message.at(0))));
        });
    }

    void disconnected(dross::error) override
    {
        change([this] {
            _log.push_back("disconnected");
        });
    }

    // Read inside a condition given to wait_for(), which holds the lock.
    const std::vector<std::string>& log_held() const
    {
        return _log;
    }

    std::vector<std::string> log()
    {
        std::vector<std::string> copy;
        change([this, &copy] {
            copy = _log;
        });
        return copy;
    }

private:
    std::vector<std::string> _log;
};

std::vector<std::byte> one_byte(int value)
{
    return { static_cast<std::byte>(value) };
}

TEST(connector_part_test, what_comes_before_connect_returns_is_told_after_connected)
{
    auto part = std::make_shared<scripted_part>([](int, const auto& events) -> std::expected<std::shared_ptr<dross::ipc::message_channel>, dross::error> {
        events->received(one_byte(1));
        events->received(one_byte(2));
        return std::make_shared<counted_channel>();
    });
    auto above = std::make_shared<told>();
    dross::connector link{ part, above };
    link.start();
    ASSERT_TRUE(above->wait_for([&] {
        return above->log_held().size() == 3;
    }));
    EXPECT_EQ(above->log(), (std::vector<std::string>{ "connected", "received 1", "received 2" }));
}

TEST(connector_part_test, an_end_before_connect_returns_is_told_after_connected_and_connects_again)
{
    auto part = std::make_shared<scripted_part>([](int now, const auto& events) -> std::expected<std::shared_ptr<dross::ipc::message_channel>, dross::error> {
        if (now == 1) {
            events->received(one_byte(1));
            events->closed({});
        }
        return std::make_shared<counted_channel>();
    });
    auto above = std::make_shared<told>();
    dross::connector link{ part, above };
    link.start();
    ASSERT_TRUE(above->wait_for([&] {
        return above->log_held().size() == 4;
    }));
    EXPECT_EQ(above->log(), (std::vector<std::string>{ "connected", "received 1", "disconnected", "connected" }));
}

TEST(connector_part_test, a_failed_try_is_not_told_and_is_tried_again)
{
    auto part = std::make_shared<scripted_part>([](int now, const auto&) -> std::expected<std::shared_ptr<dross::ipc::message_channel>, dross::error> {
        if (now == 1) {
            return std::unexpected(dross::error{ std::errc::connection_refused });
        }
        return std::make_shared<counted_channel>();
    });
    auto above = std::make_shared<told>();
    dross::connector link{ part, above };
    link.start();
    ASSERT_TRUE(above->wait_for([&] {
        return above->log_held().size() == 1;
    }));
    EXPECT_EQ(above->log(), std::vector<std::string>{ "connected" });
    EXPECT_TRUE(part->wait_for([&] {
        return part->tries_held() == 2;
    }));
}

TEST(connector_part_test, nothing_is_told_once_the_connector_is_gone)
{
    auto channel = std::make_shared<counted_channel>();
    auto part = std::make_shared<scripted_part>([channel](int, const auto&) -> std::expected<std::shared_ptr<dross::ipc::message_channel>, dross::error> {
        return channel;
    });
    auto above = std::make_shared<told>();
    auto link = std::make_unique<dross::connector>(part, above);
    link->start();
    ASSERT_TRUE(above->wait_for([&] {
        return above->log_held().size() == 1;
    }));
    const auto events = part->last_events();

    link.reset();
    EXPECT_EQ(channel->closes.load(), 1);
    events->received(one_byte(9));
    events->closed({});
    EXPECT_EQ(above->log(), std::vector<std::string>{ "connected" });
}

// Runs a connector over a part that tells received() after closed(), and
// waits for it to stop the program.
void tell_after_the_end()
{
    auto part = std::make_shared<scripted_part>([](int, const auto& events) -> std::expected<std::shared_ptr<dross::ipc::message_channel>, dross::error> {
        events->closed({});
        events->received(one_byte(1));
        return std::make_shared<counted_channel>();
    });
    auto above = std::make_shared<told>();
    dross::connector link{ part, above };
    link.start();
    (void)above->wait_for([] {
        return false;
    });
}

TEST(connector_part_test, a_part_that_tells_after_the_end_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH(tell_after_the_end(), "received: called after closed");
}

// Owns its connector, as a remote broadcaster does, and holds received()
// until the test lets it go on.
class owning_above final : public dross::connector_events, public waitable {
public:
    void connected() override
    {
        change([this] {
            _connected = true;
        });
    }

    void received(std::vector<std::byte>) override
    {
        change([this] {
            _inside = true;
        });
        wait_for([this] {
            return _go_on;
        });
    }

    void disconnected(dross::error) override
    {
    }

    void go_on()
    {
        change([this] {
            _go_on = true;
        });
    }

    // Read inside a condition given to wait_for(), which holds the lock.
    bool connected_held() const
    {
        return _connected;
    }

    bool inside_held() const
    {
        return _inside;
    }

    std::unique_ptr<dross::connector> link;

private:
    bool _connected{ false };
    bool _inside{ false };
    bool _go_on{ false };
};

TEST(connector_part_test, the_layer_above_may_go_for_good_while_the_part_tells_it)
{
    auto part = std::make_shared<scripted_part>([](int, const auto&) -> std::expected<std::shared_ptr<dross::ipc::message_channel>, dross::error> {
        return std::make_shared<counted_channel>();
    });
    auto above = std::make_shared<owning_above>();
    above->link = std::make_unique<dross::connector>(part, above);
    above->link->start();
    ASSERT_TRUE(above->wait_for([&] {
        return above->connected_held();
    }));
    const auto events = part->last_events();

    // The part's thread tells, and the last hold on the layer above is the
    // one taken for the call.
    std::mutex lock;
    std::condition_variable changed;
    bool told = false;
    std::thread telling{ [&] {
        events->received(one_byte(1));
        const std::lock_guard guard{ lock };
        told = true;
        changed.notify_all();
    } };
    ASSERT_TRUE(above->wait_for([&] {
        return above->inside_held();
    }));
    const std::weak_ptr<owning_above> watched = above;
    const auto keep = above.get();
    above.reset();
    keep->go_on();

    std::unique_lock guard{ lock };
    const bool ended = changed.wait_for(guard, 5s, [&] {
        return told;
    });
    guard.unlock();
    EXPECT_TRUE(ended);
    if (! ended) {
        telling.detach();
        return;
    }
    telling.join();
    EXPECT_TRUE(watched.expired());
}

}  // namespace
