#include "dross/notification/error.h"
#include "notification/frame.h"
#include "notification/unix_socket.h"
#include "notification/unix_transport.h"
#include "socket_directory.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

using namespace std::chrono_literals;

using unix_transport_test = dross_test::socket_directory_test;

// What a transport told its layer above, in order.
struct entry final {
    enum class kind {
        connected,
        received,
        closed
    };

    kind what;
    dross::connection_id id;
    std::vector<std::byte> message;
    dross::error reason;
};

class recorder final : public dross::transport_events {
public:
    void connected(const dross::connection_id id) override
    {
        record(entry{ entry::kind::connected, id, {}, {} });
    }

    void received(const dross::connection_id id, std::vector<std::byte> message) override
    {
        record(entry{ entry::kind::received, id, std::move(message), {} });
    }

    void closed(const dross::connection_id id, dross::error reason) override
    {
        record(entry{ entry::kind::closed, id, {}, std::move(reason) });
    }

    // Runs on the transport's thread after each entry is recorded.
    void on_entry(std::function<void(const entry&)> action)
    {
        const std::lock_guard guard{ _lock };
        _action = std::move(action);
    }

    // Waits until ready holds for the entries, for up to five seconds.
    bool wait_for(const std::function<bool(const std::vector<entry>&)>& ready)
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, 5s, [&] {
            return ready(_entries);
        });
    }

    std::vector<entry> entries()
    {
        const std::lock_guard guard{ _lock };
        return _entries;
    }

private:
    void record(entry made)
    {
        std::function<void(const entry&)> action;
        {
            const std::lock_guard guard{ _lock };
            _entries.push_back(made);
            action = _action;
        }
        _changed.notify_all();
        if (action) {
            action(made);
        }
    }

    std::mutex _lock;
    std::condition_variable _changed;
    std::vector<entry> _entries;
    std::function<void(const entry&)> _action;
};

std::size_t count_of(const std::vector<entry>& entries, const entry::kind what)
{
    std::size_t count = 0;
    for (const auto& each : entries) {
        count += (each.what == what) ? 1 : 0;
    }
    return count;
}

std::optional<entry> first_of(const std::vector<entry>& entries, const entry::kind what)
{
    for (const auto& each : entries) {
        if (each.what == what) {
            return each;
        }
    }
    return std::nullopt;
}

bool has(const std::vector<entry>& entries, const entry::kind what, const std::size_t count = 1)
{
    return count_of(entries, what) >= count;
}

std::vector<std::byte> bytes_of(const std::initializer_list<int> values)
{
    std::vector<std::byte> result;
    for (const int each : values) {
        result.push_back(static_cast<std::byte>(each));
    }
    return result;
}

// A listening transport and one connected to it, each with its recorder.
struct linked final {
    recorder server_events;
    recorder client_events;
    std::unique_ptr<dross::unix_transport> server;
    std::unique_ptr<dross::unix_transport> client;
    dross::connection_id server_side{};
    dross::connection_id client_side{};
};

void link(linked& both, const std::string& name)
{
    auto server = dross::unix_transport::listen(dross::path{ name }, both.server_events);
    ASSERT_TRUE(server.has_value());
    both.server = std::move(*server);
    auto client = dross::unix_transport::connect(dross::path{ name }, both.client_events);
    ASSERT_TRUE(client.has_value());
    both.client = std::move(*client);

    const auto connected = [](const std::vector<entry>& entries) {
        return has(entries, entry::kind::connected);
    };
    ASSERT_TRUE(both.server_events.wait_for(connected));
    ASSERT_TRUE(both.client_events.wait_for(connected));
    both.server_side = first_of(both.server_events.entries(), entry::kind::connected)->id;
    both.client_side = first_of(both.client_events.entries(), entry::kind::connected)->id;
}

// Connects a plain socket, so a test can send bytes no transport would.
dross::socket_handle raw_peer(recorder& server_events, const std::string& name)
{
    auto connected = dross::unix_connect(dross::path{ name });
    EXPECT_TRUE(connected.has_value());
    EXPECT_TRUE(server_events.wait_for([](const std::vector<entry>& entries) {
        return has(entries, entry::kind::connected);
    }));
    return connected ? std::move(*connected) : dross::socket_handle{};
}

std::optional<dross::error> reason_closed(recorder& events)
{
    if (! events.wait_for([](const std::vector<entry>& entries) {
        return has(entries, entry::kind::closed);
    })) {
        return std::nullopt;
    }
    return first_of(events.entries(), entry::kind::closed)->reason;
}

}  // namespace

TEST_F(unix_transport_test, messages_travel_both_ways)
{
    linked both;
    ASSERT_NO_FATAL_FAILURE(link(both, at("s")));

    ASSERT_TRUE(both.client->send(both.client_side, bytes_of({ 1, 2, 3 })).has_value());
    ASSERT_TRUE(both.server_events.wait_for([](const std::vector<entry>& entries) {
        return has(entries, entry::kind::received);
    }));
    const auto at_server = first_of(both.server_events.entries(), entry::kind::received);
    EXPECT_EQ(at_server->id, both.server_side);
    EXPECT_EQ(at_server->message, bytes_of({ 1, 2, 3 }));

    ASSERT_TRUE(both.server->send(both.server_side, bytes_of({ 4 })).has_value());
    ASSERT_TRUE(both.client_events.wait_for([](const std::vector<entry>& entries) {
        return has(entries, entry::kind::received);
    }));
    EXPECT_EQ(first_of(both.client_events.entries(), entry::kind::received)->message, bytes_of({ 4 }));
}

TEST_F(unix_transport_test, messages_arrive_in_the_order_they_were_sent)
{
    linked both;
    ASSERT_NO_FATAL_FAILURE(link(both, at("s")));

    constexpr int kCount = 1000;
    for (int i = 0; i < kCount; ++i) {
        ASSERT_TRUE(both.client->send(both.client_side, bytes_of({ i / 256, i % 256 })).has_value());
    }
    ASSERT_TRUE(both.server_events.wait_for([](const std::vector<entry>& entries) {
        return has(entries, entry::kind::received, kCount);
    }));
    int next = 0;
    for (const auto& each : both.server_events.entries()) {
        if (each.what == entry::kind::received) {
            EXPECT_EQ(each.message, bytes_of({ next / 256, next % 256 }));
            ++next;
        }
    }
}

TEST_F(unix_transport_test, a_message_of_the_largest_size_arrives_whole)
{
    linked both;
    ASSERT_NO_FATAL_FAILURE(link(both, at("s")));

    std::vector<std::byte> largest(dross::max_message_size);
    for (std::size_t i = 0; i < largest.size(); ++i) {
        largest[i] = static_cast<std::byte>(i % 251);
    }
    ASSERT_TRUE(both.client->send(both.client_side, largest).has_value());
    ASSERT_TRUE(both.server_events.wait_for([](const std::vector<entry>& entries) {
        return has(entries, entry::kind::received);
    }));
    EXPECT_EQ(first_of(both.server_events.entries(), entry::kind::received)->message, largest);
}

TEST_F(unix_transport_test, a_message_too_large_is_refused_and_the_connection_stays)
{
    linked both;
    ASSERT_NO_FATAL_FAILURE(link(both, at("s")));

    const std::vector<std::byte> over(dross::max_message_size + 1);
    const auto refused = both.client->send(both.client_side, over);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), dross::notification_errc::message_too_large);

    ASSERT_TRUE(both.client->send(both.client_side, bytes_of({ 9 })).has_value());
    ASSERT_TRUE(both.server_events.wait_for([](const std::vector<entry>& entries) {
        return has(entries, entry::kind::received);
    }));
    EXPECT_FALSE(has(both.server_events.entries(), entry::kind::closed));
}

TEST_F(unix_transport_test, sending_on_a_connection_that_is_not_there_fails)
{
    linked both;
    ASSERT_NO_FATAL_FAILURE(link(both, at("s")));

    const auto sent = both.server->send(both.server_side + 100, bytes_of({ 1 }));
    ASSERT_FALSE(sent.has_value());
    EXPECT_EQ(sent.error(), dross::error(std::errc::not_connected));
}

TEST_F(unix_transport_test, a_peer_that_goes_is_closed_without_a_reason)
{
    linked both;
    ASSERT_NO_FATAL_FAILURE(link(both, at("s")));

    both.client.reset();
    const auto reason = reason_closed(both.server_events);
    ASSERT_TRUE(reason.has_value());
    EXPECT_FALSE(static_cast<bool>(*reason));
}

TEST_F(unix_transport_test, a_peer_that_goes_in_the_middle_of_a_message_is_closed_with_message_cut_short)
{
    recorder events;
    auto server = dross::unix_transport::listen(dross::path{ at("s") }, events);
    ASSERT_TRUE(server.has_value());
    {
        const auto peer = raw_peer(events, at("s"));
        ASSERT_TRUE(dross::send_some(peer.get(), bytes_of({ 0, 0, 0, 10, 1, 2, 3 })).has_value());
    }
    const auto reason = reason_closed(events);
    ASSERT_TRUE(reason.has_value());
    EXPECT_EQ(*reason, dross::notification_errc::message_cut_short);
    EXPECT_FALSE(has(events.entries(), entry::kind::received));
}

TEST_F(unix_transport_test, a_length_over_the_limit_closes_the_connection_with_message_too_large)
{
    recorder events;
    auto server = dross::unix_transport::listen(dross::path{ at("s") }, events);
    ASSERT_TRUE(server.has_value());
    const auto peer = raw_peer(events, at("s"));
    ASSERT_TRUE(dross::send_some(peer.get(), bytes_of({ 0, 1, 0, 1 })).has_value());

    const auto reason = reason_closed(events);
    ASSERT_TRUE(reason.has_value());
    EXPECT_EQ(*reason, dross::notification_errc::message_too_large);
}

TEST_F(unix_transport_test, a_peer_of_another_user_is_closed_without_being_connected)
{
    recorder events;
    auto server = dross::unix_transport::listen(dross::path{ at("s") }, events, ::geteuid() + 1);
    ASSERT_TRUE(server.has_value());
    const auto peer = dross::unix_connect(dross::path{ at("s") });
    ASSERT_TRUE(peer.has_value());

    const auto reason = reason_closed(events);
    ASSERT_TRUE(reason.has_value());
    EXPECT_EQ(*reason, dross::notification_errc::peer_not_same_user);
    EXPECT_EQ(count_of(events.entries(), entry::kind::closed), 1U);
    EXPECT_FALSE(has(events.entries(), entry::kind::connected));
}

TEST_F(unix_transport_test, connecting_to_a_peer_of_another_user_fails)
{
    recorder server_events;
    auto server = dross::unix_transport::listen(dross::path{ at("s") }, server_events);
    ASSERT_TRUE(server.has_value());

    recorder client_events;
    const auto client = dross::unix_transport::connect(dross::path{ at("s") }, client_events, ::geteuid() + 1);
    ASSERT_FALSE(client.has_value());
    EXPECT_EQ(client.error(), dross::notification_errc::peer_not_same_user);
}

TEST_F(unix_transport_test, a_peer_that_does_not_read_overflows_the_queue_and_is_closed)
{
    // Holds the transport's thread in connected(), so nothing reaches the
    // socket while the queue fills.
    std::mutex gate_lock;
    std::condition_variable gate_changed;
    bool open = false;
    recorder events;
    events.on_entry([&](const entry& made) {
        if (made.what == entry::kind::connected) {
            std::unique_lock guard{ gate_lock };
            gate_changed.wait_for(guard, 5s, [&] {
                return open;
            });
        }
    });
    auto server = dross::unix_transport::listen(dross::path{ at("s") }, events);
    ASSERT_TRUE(server.has_value());
    const auto peer = raw_peer(events, at("s"));
    const dross::connection_id id = first_of(events.entries(), entry::kind::connected)->id;

    const std::vector<std::byte> block(dross::max_message_size);
    std::optional<dross::error> failure;
    int sent = 0;
    for (; (sent < 1000) && ! failure; ++sent) {
        if (auto result = (*server)->send(id, block); ! result) {
            failure = result.error();
        }
    }
    {
        const std::lock_guard guard{ gate_lock };
        open = true;
    }
    gate_changed.notify_all();
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(*failure, dross::notification_errc::queue_overflow);
    // 15 framed messages fit in 1 MiB, so the 16th is the first to fail.
    EXPECT_EQ(sent, 16);

    const auto reason = reason_closed(events);
    ASSERT_TRUE(reason.has_value());
    EXPECT_EQ(*reason, dross::notification_errc::queue_overflow);
}

TEST_F(unix_transport_test, close_ends_the_connection_on_both_sides)
{
    linked both;
    ASSERT_NO_FATAL_FAILURE(link(both, at("s")));

    both.server->close(both.server_side);
    const auto at_server = reason_closed(both.server_events);
    ASSERT_TRUE(at_server.has_value());
    EXPECT_FALSE(static_cast<bool>(*at_server));
    const auto at_client = reason_closed(both.client_events);
    ASSERT_TRUE(at_client.has_value());
    EXPECT_FALSE(static_cast<bool>(*at_client));

    const auto sent = both.server->send(both.server_side, bytes_of({ 1 }));
    ASSERT_FALSE(sent.has_value());
    EXPECT_EQ(sent.error(), dross::error(std::errc::not_connected));
}

TEST_F(unix_transport_test, each_connection_is_connected_and_then_closed_once)
{
    recorder events;
    auto server = dross::unix_transport::listen(dross::path{ at("s") }, events);
    ASSERT_TRUE(server.has_value());
    std::vector<recorder> client_events(3);
    std::vector<std::unique_ptr<dross::unix_transport>> clients;
    for (auto& each : client_events) {
        auto client = dross::unix_transport::connect(dross::path{ at("s") }, each);
        ASSERT_TRUE(client.has_value());
        clients.push_back(std::move(*client));
    }
    ASSERT_TRUE(events.wait_for([](const std::vector<entry>& entries) {
        return has(entries, entry::kind::connected, 3);
    }));

    // The first is closed here while its peer goes at the same moment.
    const dross::connection_id first = first_of(events.entries(), entry::kind::connected)->id;
    (*server)->close(first);
    clients.clear();
    ASSERT_TRUE(events.wait_for([](const std::vector<entry>& entries) {
        return has(entries, entry::kind::closed, 3);
    }));
    std::this_thread::sleep_for(50ms);

    const auto entries = events.entries();
    EXPECT_EQ(count_of(entries, entry::kind::closed), 3U);
    for (const auto& each : entries) {
        if (each.what != entry::kind::connected) {
            continue;
        }
        std::vector<entry::kind> order;
        for (const auto& other : entries) {
            if (other.id == each.id) {
                order.push_back(other.what);
            }
        }
        EXPECT_EQ(order, (std::vector{ entry::kind::connected, entry::kind::closed }));
    }
}

TEST_F(unix_transport_test, destroyed_from_another_thread_it_calls_nothing_more_and_removes_its_file)
{
    linked both;
    ASSERT_NO_FATAL_FAILURE(link(both, at("s")));

    both.server.reset();
    const auto seen = both.server_events.entries().size();
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::symlink_status(at("s"))));

    EXPECT_TRUE(reason_closed(both.client_events).has_value());
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(both.server_events.entries().size(), seen);
}

TEST_F(unix_transport_test, destroyed_from_its_own_event_it_calls_nothing_more_and_removes_its_file)
{
    recorder server_events;
    std::mutex owner_lock;
    std::unique_ptr<dross::unix_transport> server;
    std::condition_variable owner_changed;
    bool ended = false;
    bool event_returned = false;
    server_events.on_entry([&](const entry& made) {
        if (made.what != entry::kind::received) {
            return;
        }
        std::unique_ptr<dross::unix_transport> ending;
        {
            const std::lock_guard guard{ owner_lock };
            if (ended) {
                return;
            }
            ended = true;
            ending = std::move(server);
        }
        ending.reset();
        const std::lock_guard guard{ owner_lock };
        event_returned = true;
        owner_changed.notify_all();
    });
    {
        auto made = dross::unix_transport::listen(dross::path{ at("s") }, server_events);
        ASSERT_TRUE(made.has_value());
        const std::lock_guard guard{ owner_lock };
        server = std::move(*made);
    }
    const auto peer = raw_peer(server_events, at("s"));

    // Several messages in one write, so more are waiting when the first
    // one's event ends the transport.
    std::vector<std::byte> stream;
    for (int i = 0; i < 10; ++i) {
        const auto framed = dross::frame_message(bytes_of({ i }));
        ASSERT_TRUE(framed.has_value());
        stream.insert(stream.end(), framed->begin(), framed->end());
    }
    ASSERT_EQ(dross::send_some(peer.get(), stream), stream.size());
    // The thread let go is not waited for, so the event says when it ends.
    {
        std::unique_lock guard{ owner_lock };
        ASSERT_TRUE(owner_changed.wait_for(guard, 5s, [&] {
            return event_returned;
        }));
    }
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(count_of(server_events.entries(), entry::kind::received), 1U);

    bool removed = false;
    for (int i = 0; (i < 500) && ! removed; ++i) {
        removed = ! std::filesystem::exists(std::filesystem::symlink_status(at("s")));
        std::this_thread::sleep_for(10ms);
    }
    EXPECT_TRUE(removed);
}

TEST_F(unix_transport_test, connecting_where_no_one_listens_fails)
{
    recorder events;
    const auto client = dross::unix_transport::connect(dross::path{ at("none") }, events);
    ASSERT_FALSE(client.has_value());
    EXPECT_EQ(client.error(), dross::error(std::errc::no_such_file_or_directory));
}
