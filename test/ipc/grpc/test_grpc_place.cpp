#include "../../thread/test_support.h"
#include "dross/ipc/error.h"
#include "dross/ipc/message_acceptor.h"
#include "dross/ipc/message_channel.h"
#include "dross/ipc/message_connector.h"
#include "ipc/grpc/cleanup_thread.h"
#include "ipc/grpc/grpc_place.h"
#include "ipc/part_calls.h"
#include "ipc/part_recorder.h"
#include "ipc/socket_directory.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <expected>
#include <functional>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
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
using bytes = dross_test::bytes;

using dross_test::bytes_of;
using dross_test::kPatience;
using dross_test::recorder;

// The gRPC part does not stop on its own.
class acceptance final : public dross_test::acceptance {
public:
    void stopped(dross::error reason) override
    {
        dross_test::acceptance::stopped(std::move(reason));
        ADD_FAILURE() << "the gRPC part does not stop on its own";
    }
};

// Holds back what it is given until let go.
class gate {
public:
    // Returns whether it was let go in time.
    bool wait()
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, kPatience, [&] {
            return _open;
        });
    }

    void open()
    {
        const std::lock_guard guard{ _lock };
        _open = true;
        _changed.notify_all();
    }

private:
    std::mutex _lock;
    std::condition_variable _changed;
    bool _open{ false };
};

// Owns an acceptor, and opens a gate once it has destroyed it.
class acceptor_owner {
public:
    acceptor_owner(std::unique_ptr<dross::ipc::message_acceptor> acceptor, std::shared_ptr<gate> destroyed)
        : _acceptor{ std::move(acceptor) }
        , _destroyed{ std::move(destroyed) }
    {
    }

    ~acceptor_owner()
    {
        _acceptor.reset();
        _destroyed->open();
    }

private:
    std::unique_ptr<dross::ipc::message_acceptor> _acceptor;
    const std::shared_ptr<gate> _destroyed;
};

// The events of a connection, which own what they are given.
class owning_events final : public dross::ipc::message_channel_events {
public:
    explicit owning_events(std::shared_ptr<acceptor_owner> owned)
        : _owned{ std::move(owned) }
    {
    }

    void received(std::vector<std::byte>) override
    {
    }

    void closed(dross::error) override
    {
    }

private:
    const std::shared_ptr<acceptor_owner> _owned;
};

// Accepts one connection, whose events take what is handed to it.
class handing_acceptance final : public dross::ipc::message_acceptor_events {
public:
    void hand(std::shared_ptr<acceptor_owner> owned)
    {
        const std::lock_guard guard{ _lock };
        _owned = std::move(owned);
    }

    std::shared_ptr<dross::ipc::message_channel_events> accepted(std::shared_ptr<dross::ipc::message_channel>) override
    {
        const std::lock_guard guard{ _lock };
        return std::make_shared<owning_events>(std::move(_owned));
    }

    void stopped(dross::error) override
    {
    }

private:
    std::mutex _lock;
    std::shared_ptr<acceptor_owner> _owned;
};

class grpc_place_test : public dross_test::socket_directory_test {
protected:
    std::string address() const
    {
        return "unix:" + at("place.sock");
    }

    std::expected<std::unique_ptr<dross::ipc::message_acceptor>, dross::error> listen(std::shared_ptr<dross::ipc::message_acceptor_events> events) const
    {
        return dross::ipc::grpc_listen_at(address(), ::grpc::InsecureServerCredentials(), std::move(events));
    }

    std::shared_ptr<dross::ipc::message_connector> connector() const
    {
        return dross::ipc::grpc_connector_to(address(), ::grpc::InsecureChannelCredentials(), kPatience);
    }
};

TEST_F(grpc_place_test, messages_cross_both_ways_in_order)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());

    auto mine = std::make_shared<recorder>();
    auto channel = connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE((*channel)->send(bytes_of({ 1 })).has_value());
    ASSERT_TRUE((*channel)->send(bytes_of({ 2, 3 })).has_value());
    ASSERT_TRUE(above->wait_for_accepted(1));
    ASSERT_TRUE(above->events(0)->wait_for_messages(2));
    EXPECT_EQ(above->events(0)->messages(), (std::vector<bytes>{ bytes_of({ 1 }), bytes_of({ 2, 3 }) }));

    ASSERT_TRUE(above->channel(0)->send(bytes_of({ 4 })).has_value());
    ASSERT_TRUE(mine->wait_for_messages(1));
    EXPECT_EQ(mine->messages(), std::vector<bytes>{ bytes_of({ 4 }) });
}

TEST_F(grpc_place_test, many_messages_from_many_connections_arrive_whole_and_in_order)
{
    constexpr int kConnections = 4;
    constexpr int kMessages = 300;
    auto above = std::make_shared<acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());

    std::vector<std::shared_ptr<dross::ipc::message_channel>> channels;
    for (int c = 0; c < kConnections; ++c) {
        auto channel = connector()->connect(std::make_shared<recorder>());
        ASSERT_TRUE(channel.has_value());
        channels.push_back(*channel);
    }
    std::vector<std::thread> senders;
    for (int c = 0; c < kConnections; ++c) {
        senders.emplace_back([&channel = channels[c], c] {
            for (int m = 0; m < kMessages; ++m) {
                (void)channel->send(bytes_of({ c, m % 256, m / 256 }));
            }
        });
    }
    for (auto& sender : senders) {
        sender.join();
    }

    ASSERT_TRUE(above->wait_for_accepted(kConnections));
    for (int c = 0; c < kConnections; ++c) {
        ASSERT_TRUE(above->events(c)->wait_for_messages(kMessages));
        const auto got = above->events(c)->messages();
        ASSERT_EQ(got.size(), std::size_t{ kMessages });
        const int from = static_cast<int>(got.front().front());
        for (int m = 0; m < kMessages; ++m) {
            EXPECT_EQ(got[m], bytes_of({ from, m % 256, m / 256 }));
        }
    }
}

TEST_F(grpc_place_test, connecting_where_nothing_listens_fails_after_the_try)
{
    auto mine = std::make_shared<recorder>();
    const auto start = std::chrono::steady_clock::now();
    const auto channel = dross::ipc::grpc_connector_to(address(), ::grpc::InsecureChannelCredentials(), 200ms)->connect(mine);
    const auto took = std::chrono::steady_clock::now() - start;
    ASSERT_FALSE(channel.has_value());
    EXPECT_EQ(channel.error(), dross::error(std::errc::timed_out));
    EXPECT_GE(took, 200ms);
    EXPECT_LT(took, kPatience);
    EXPECT_EQ(mine->closes(), 0);
}

TEST_F(grpc_place_test, listening_where_grpc_cannot_fails)
{
    const auto acceptor = dross::ipc::grpc_listen_at("unix:" + at("missing/place.sock"), ::grpc::InsecureServerCredentials(), std::make_shared<acceptance>());
    ASSERT_FALSE(acceptor.has_value());
    EXPECT_EQ(acceptor.error(), std::errc::address_not_available);
}

TEST_F(grpc_place_test, listening_fails_when_the_cleanup_thread_does_not_start)
{
    dross::ipc::cleanup_thread_access::forget();
    {
        const dross_test::failing_thread_starts failing{ 1 };
        const auto acceptor = listen(std::make_shared<acceptance>());
        ASSERT_FALSE(acceptor.has_value());
        EXPECT_EQ(acceptor.error(), dross::error(std::errc::resource_unavailable_try_again));
    }
    EXPECT_TRUE(listen(std::make_shared<acceptance>()).has_value());
}

TEST_F(grpc_place_test, connecting_fails_when_the_cleanup_thread_does_not_start)
{
    auto acceptor = listen(std::make_shared<acceptance>());
    ASSERT_TRUE(acceptor.has_value());
    dross::ipc::cleanup_thread_access::forget();

    auto mine = std::make_shared<recorder>();
    {
        const dross_test::failing_thread_starts failing{ 1 };
        const auto channel = connector()->connect(mine);
        ASSERT_FALSE(channel.has_value());
        EXPECT_EQ(channel.error(), dross::error(std::errc::resource_unavailable_try_again));
    }
    EXPECT_EQ(mine->closes(), 0);
    EXPECT_TRUE(connector()->connect(std::make_shared<recorder>()).has_value());
}

TEST_F(grpc_place_test, the_cleanup_thread_is_not_among_the_threads_dross_lists)
{
    dross::ipc::cleanup_thread_access::forget();
    const std::vector<dross::thread> before = dross::all_threads();
    auto acceptor = listen(std::make_shared<acceptance>());
    ASSERT_TRUE(acceptor.has_value());
    EXPECT_EQ(dross_test::threads_not_in(before), 0U);
}

TEST_F(grpc_place_test, closing_from_the_connecting_side_ends_both_once_in_order)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto mine = std::make_shared<recorder>();
    auto channel = connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE((*channel)->send(bytes_of({ 1 })).has_value());
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

TEST_F(grpc_place_test, closing_from_the_accepting_side_ends_both_in_order)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto mine = std::make_shared<recorder>();
    auto channel = connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE((*channel)->send(bytes_of({ 1 })).has_value());
    ASSERT_TRUE(above->wait_for_accepted(1));

    above->channel(0)->close();
    ASSERT_TRUE(above->events(0)->wait_for_close());
    ASSERT_TRUE(mine->wait_for_close());
    EXPECT_EQ(above->events(0)->reason(), dross::error{});
    EXPECT_EQ(mine->reason(), dross::error{});
    const auto refused = above->channel(0)->send(bytes_of({ 1 }));
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), dross::error(std::errc::not_connected));
}

TEST_F(grpc_place_test, the_accepting_side_receives_nothing_once_it_closed)
{
    auto above = std::make_shared<acceptance>();
    above->on_receive = [&above] {
        above->channel(0)->close();
    };
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto mine = std::make_shared<recorder>();
    auto channel = connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    // Sends after the close may be refused.
    for (int i = 0; i < 20; ++i) {
        (void)(*channel)->send(bytes_of({ 1 }));
    }

    ASSERT_TRUE(above->wait_for_accepted(1));
    ASSERT_TRUE(above->events(0)->wait_for_close());
    EXPECT_EQ(above->events(0)->messages().size(), 1u);
}

TEST_F(grpc_place_test, the_connecting_side_receives_nothing_once_it_closed)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto mine = std::make_shared<recorder>();
    auto channel = connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    mine->on_receive = [&channel] {
        (*channel)->close();
    };
    ASSERT_TRUE((*channel)->send(bytes_of({ 1 })).has_value());
    ASSERT_TRUE(above->wait_for_accepted(1));
    // Sends after the close may be refused.
    for (int i = 0; i < 20; ++i) {
        (void)above->channel(0)->send(bytes_of({ 2 }));
    }

    ASSERT_TRUE(mine->wait_for_close());
    EXPECT_EQ(mine->messages().size(), 1u);
}

TEST_F(grpc_place_test, letting_the_connecting_channel_go_keeps_the_connection)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto mine = std::make_shared<recorder>();
    {
        auto channel = connector()->connect(mine);
        ASSERT_TRUE(channel.has_value());
        ASSERT_TRUE((*channel)->send(bytes_of({ 1 })).has_value());
    }
    ASSERT_TRUE(above->wait_for_accepted(1));

    ASSERT_TRUE(above->channel(0)->send(bytes_of({ 5 })).has_value());
    ASSERT_TRUE(mine->wait_for_messages(1));

    above->channel(0)->close();
    ASSERT_TRUE(mine->wait_for_close());
}

TEST_F(grpc_place_test, a_message_over_the_largest_fails_and_the_connection_stays)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto channel = connector()->connect(std::make_shared<recorder>());
    ASSERT_TRUE(channel.has_value());

    const bytes over(64 * 1024 + 1, std::byte{ 1 });
    const auto refused = (*channel)->send(over);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), dross::ipc_errc::message_too_large);

    const bytes largest(64 * 1024, std::byte{ 2 });
    ASSERT_TRUE((*channel)->send(largest).has_value());
    ASSERT_TRUE(above->wait_for_accepted(1));
    ASSERT_TRUE(above->events(0)->wait_for_messages(1));
    EXPECT_EQ(above->events(0)->messages(), std::vector<bytes>{ largest });
}

TEST_F(grpc_place_test, a_queue_that_overflows_closes_the_connection)
{
    gate held;
    auto above = std::make_shared<acceptance>();
    above->on_receive = [&held] {
        held.wait();
    };
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto mine = std::make_shared<recorder>();
    auto channel = connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());

    // The peer takes no more once its events hold the first message, so
    // what is sent waits, until there is more than a connection holds.
    const bytes large(64 * 1024, std::byte{ 3 });
    std::optional<dross::error> refused;
    for (int sent = 0; sent < 10'000 && ! refused; ++sent) {
        if (auto result = (*channel)->send(large); ! result) {
            refused = result.error();
        }
    }
    held.open();
    ASSERT_TRUE(refused.has_value());
    EXPECT_EQ(*refused, dross::ipc_errc::queue_overflow);
    ASSERT_TRUE(mine->wait_for_close());
    EXPECT_EQ(mine->reason(), dross::error(dross::ipc_errc::queue_overflow));
    const auto after = (*channel)->send(bytes_of({ 1 }));
    ASSERT_FALSE(after.has_value());
    EXPECT_EQ(after.error(), dross::error(std::errc::not_connected));
}

TEST_F(grpc_place_test, destroying_the_acceptor_ends_the_peers_without_closed_on_its_own_side)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto mine = std::make_shared<recorder>();
    auto channel = connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE((*channel)->send(bytes_of({ 1 })).has_value());
    ASSERT_TRUE(above->wait_for_accepted(1));

    acceptor->reset();
    ASSERT_TRUE(mine->wait_for_close());
    EXPECT_NE(mine->reason(), dross::error{});
    EXPECT_EQ(above->events(0)->closes(), 0);
    const auto refused = above->channel(0)->send(bytes_of({ 1 }));
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), dross::error(std::errc::not_connected));
}

TEST_F(grpc_place_test, the_acceptor_can_be_destroyed_from_inside_accepted)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    std::mutex lock;
    std::unique_ptr<dross::ipc::message_acceptor> held = std::move(*acceptor);
    above->on_accept = [&] {
        const std::lock_guard guard{ lock };
        held.reset();
    };

    auto mine = std::make_shared<recorder>();
    auto channel = connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE((*channel)->send(bytes_of({ 1 })).has_value());
    ASSERT_TRUE(mine->wait_for_close());
    const std::lock_guard guard{ lock };
    EXPECT_EQ(held, nullptr);
    EXPECT_EQ(above->events(0)->messages().size(), 0u);
}

TEST_F(grpc_place_test, the_acceptor_can_be_destroyed_from_inside_received)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    std::mutex lock;
    std::unique_ptr<dross::ipc::message_acceptor> held = std::move(*acceptor);
    std::optional<std::expected<void, dross::error>> sent_after;
    above->on_receive = [&] {
        const std::lock_guard guard{ lock };
        held.reset();
        // The acceptor is gone, though the call it went inside of is not
        // over yet.
        sent_after = above->channel(0)->send(bytes_of({ 3 }));
    };

    auto mine = std::make_shared<recorder>();
    auto channel = connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE((*channel)->send(bytes_of({ 1 })).has_value());
    ASSERT_TRUE((*channel)->send(bytes_of({ 2 })).has_value());
    ASSERT_TRUE(mine->wait_for_close());
    const std::lock_guard guard{ lock };
    EXPECT_EQ(held, nullptr);
    ASSERT_TRUE(sent_after.has_value());
    ASSERT_FALSE(sent_after->has_value());
    EXPECT_EQ(sent_after->error(), dross::error(std::errc::not_connected));
    EXPECT_EQ(mine->messages().size(), 0u);
    EXPECT_EQ(above->events(0)->messages(), std::vector<bytes>{ bytes_of({ 1 }) });
    EXPECT_EQ(above->events(0)->closes(), 0);
}

TEST_F(grpc_place_test, the_acceptor_can_be_destroyed_by_letting_the_events_of_a_connection_it_accepted_go)
{
    auto above = std::make_shared<handing_acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto destroyed = std::make_shared<gate>();
    above->hand(std::make_shared<acceptor_owner>(std::move(*acceptor), destroyed));

    auto mine = std::make_shared<recorder>();
    auto channel = connector()->connect(mine);
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE((*channel)->send(bytes_of({ 1 })).has_value());
    (*channel)->close();
    ASSERT_TRUE(mine->wait_for_close());
    EXPECT_TRUE(destroyed->wait());
}

TEST_F(grpc_place_test, the_acceptor_can_be_destroyed_by_letting_the_events_of_a_connection_to_it_go)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto destroyed = std::make_shared<gate>();
    auto channel = connector()->connect(std::make_shared<owning_events>(std::make_shared<acceptor_owner>(std::move(*acceptor), destroyed)));
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE((*channel)->send(bytes_of({ 1 })).has_value());
    ASSERT_TRUE(above->wait_for_accepted(1));

    (*channel)->close();
    EXPECT_TRUE(destroyed->wait());
}

TEST_F(grpc_place_test, the_events_are_let_go_once_the_acceptor_is_gone)
{
    auto above = std::make_shared<acceptance>();
    auto acceptor = listen(above);
    ASSERT_TRUE(acceptor.has_value());
    auto channel = connector()->connect(std::make_shared<recorder>());
    ASSERT_TRUE(channel.has_value());
    ASSERT_TRUE((*channel)->send(bytes_of({ 1 })).has_value());
    ASSERT_TRUE(above->wait_for_accepted(1));

    const std::weak_ptr<acceptance> acceptor_events = above;
    const std::weak_ptr<recorder> channel_events = above->events(0);
    above.reset();
    acceptor->reset();
    EXPECT_TRUE(acceptor_events.expired());
    EXPECT_EQ(channel_events.use_count(), 0);
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

TEST_F(grpc_place_test, accepted_that_returns_null_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH(
        {
            auto acceptor = listen(std::make_shared<giving_null>());
            auto channel = connector()->connect(std::make_shared<recorder>());
            if (channel) {
                (void)(*channel)->send(bytes_of({ 1 }));
            }
            std::this_thread::sleep_for(kPatience);
        },
        "message_acceptor_events::accepted: returned null");
}

}  // namespace
