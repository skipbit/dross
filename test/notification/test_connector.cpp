#include "ipc/socket_directory.h"
#include "ipc/unix_transport.h"
#include "notification/connector.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

using connector_test = dross_test::socket_directory_test;

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
    dross::connector link{ dross::path{ at("s") }, events };
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
    dross::connector link{ dross::path{ at("s") }, events };
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
    dross::connector link{ dross::path{ at("s") }, events };
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
    dross::connector link{ dross::path{ at("s") }, events };
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
    dross::connector link{ dross::path{ at("s") }, events };
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
    dross::connector link{ dross::path{ at("s") }, events };
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
    dross::connector link{ dross::path{ at("s") }, events };
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
    auto link = std::make_unique<dross::connector>(dross::path{ at("s") }, events);
    link->start();
    events->hold(std::move(link));

    EXPECT_TRUE(events->let_go());
}

TEST_F(connector_test, calls_nothing_once_the_layer_above_has_gone)
{
    peer there{ at("s") };
    auto events = std::make_shared<above>();
    dross::connector link{ dross::path{ at("s") }, std::weak_ptr<above>{ events } };
    events.reset();
    link.start();

    EXPECT_TRUE(there.joined());
}

}  // namespace
