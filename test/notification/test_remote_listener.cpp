#include "dross/ipc/unix_socket.h"
#include "dross/notification/broadcaster.h"
#include "dross/notification/delivery.h"
#include "dross/notification/error.h"
#include "dross/notification/link_event.h"
#include "dross/notification/listener.h"
#include "dross/notification/topic.h"
#include "ipc/socket_directory.h"
#include "ipc/unix_transport.h"
#include "notification/wire.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

namespace {

using namespace std::chrono_literals;

using remote_listener_test = dross_test::socket_directory_test;

inline constexpr dross::topic<std::int64_t> kCount{ "com.example.test.count" };
inline constexpr dross::topic<std::int64_t> kOther{ "com.example.test.other" };
inline constexpr dross::topic<std::string> kCountAsText{ "com.example.test.count" };

struct no_traits {
    int n;
};

inline constexpr dross::topic<no_traits> kNoTraits{ "com.example.test.no_traits" };

// What the subscribers of one listener were handed, on whichever thread.
class heard final {
public:
    void count(const std::int64_t n)
    {
        change([&] {
            _counts.push_back(n);
        });
    }

    void link(const dross::link_event& what)
    {
        change([&] {
            _links.push_back(what);
        });
    }

    bool wait_for(const std::function<bool()>& done, const std::chrono::milliseconds limit = 5s)
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, limit, done);
    }

    // Read inside a condition given to wait_for(), which holds the lock.
    const std::vector<std::int64_t>& counts_held() const
    {
        return _counts;
    }

    const std::vector<dross::link_event>& links_held() const
    {
        return _links;
    }

    std::size_t links_of(const dross::link_event::kind what) const
    {
        std::size_t found = 0;
        for (const auto& each : _links) {
            found += (each.what == what) ? 1 : 0;
        }
        return found;
    }

    std::vector<std::int64_t> counts()
    {
        const std::lock_guard guard{ _lock };
        return _counts;
    }

    std::vector<dross::link_event> links()
    {
        const std::lock_guard guard{ _lock };
        return _links;
    }

private:
    template <typename F>
    void change(F&& how)
    {
        const std::lock_guard guard{ _lock };
        how();
        _changed.notify_all();
    }

    std::mutex _lock;
    std::condition_variable _changed;
    std::vector<std::int64_t> _counts;
    std::vector<dross::link_event> _links;
};

// A listener with a subscriber on link and one on kCount, both direct. What
// they were handed is shared with them, since a subscriber's call on the
// receiving thread may still be running when the subscription goes.
struct listening final {
    explicit listening(const dross::ipc::unix_socket& place)
        : ears{ dross::listener::create(place) }
    {
        auto watched = ears.subscribe(dross::listener::link, [kept = got](const dross::link_event& what) {
            kept->link(what);
        }, dross::delivery::direct());
        auto subscribed = ears.subscribe(kCount, [kept = got](const std::int64_t n) {
            kept->count(n);
        }, dross::delivery::direct());
        EXPECT_TRUE(watched.has_value());
        EXPECT_TRUE(subscribed.has_value());
        if (watched && subscribed) {
            link = std::move(*watched);
            count = std::move(*subscribed);
        }
    }

    std::shared_ptr<heard> got{ std::make_shared<heard>() };
    dross::listener ears;
    dross::subscription link;
    dross::subscription count;
};

// Notifies value on which until got has been handed it, since a subscription
// takes effect only once it reaches the broadcaster.
bool notify_until_heard(dross::broadcaster& news, heard& got, const std::int64_t value)
{
    for (int i = 0; i < 500; ++i) {
        news.notify(kCount, value);
        const bool done = got.wait_for([&] {
            for (const auto each : got.counts_held()) {
                if (each == value) {
                    return true;
                }
            }
            return false;
        }, 10ms);
        if (done) {
            return true;
        }
    }
    return false;
}

// Stands in for a broadcaster in another process: listens at a path, keeps
// the messages it is sent, and sends what it is told to.
class fake_broadcaster final : public dross::transport_events {
public:
    explicit fake_broadcaster(const std::string& path)
    {
        auto made = dross::unix_transport::listen(dross::path{ path }, *this);
        EXPECT_TRUE(made.has_value());
        if (made) {
            _transport = std::move(*made);
            _transport->start();
        }
    }

    ~fake_broadcaster() override
    {
        _transport.reset();
    }

    bool wait_for(const std::function<bool()>& done, const std::chrono::milliseconds limit = 5s)
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, limit, done);
    }

    void send(const dross::wire::message& which)
    {
        std::optional<dross::connection_id> id;
        {
            const std::lock_guard guard{ _lock };
            id = _id;
        }
        ASSERT_TRUE(id.has_value());
        ASSERT_TRUE(_transport->send(*id, dross::wire::encode(which)).has_value());
    }

    // Read inside a condition given to wait_for(), which holds the lock.
    const std::vector<dross::wire::message>& messages_held() const
    {
        return _messages;
    }

    bool joined_held() const
    {
        return _id.has_value();
    }

    void connected(const dross::connection_id id) override
    {
        const std::lock_guard guard{ _lock };
        _id = id;
        _changed.notify_all();
    }

    void received(dross::connection_id, std::vector<std::byte> message) override
    {
        auto read = dross::wire::decode(message);
        const std::lock_guard guard{ _lock };
        if (read) {
            _messages.push_back(std::move(*read));
        }
        _changed.notify_all();
    }

    void closed(dross::connection_id, dross::error) override
    {
        const std::lock_guard guard{ _lock };
        _id.reset();
        _changed.notify_all();
    }

    void stopped(dross::error) override
    {
    }

private:
    std::mutex _lock;
    std::condition_variable _changed;
    std::unique_ptr<dross::unix_transport> _transport;
    std::optional<dross::connection_id> _id;
    std::vector<dross::wire::message> _messages;
};

std::vector<std::string> names_asked(const std::vector<dross::wire::message>& sent)
{
    std::vector<std::string> names;
    for (const auto& each : sent) {
        if (const auto* asked = std::get_if<dross::wire::subscribe>(&each)) {
            names.push_back("+" + asked->name);
        } else if (const auto* dropped = std::get_if<dross::wire::unsubscribe>(&each)) {
            names.push_back("-" + dropped->name);
        }
    }
    return names;
}

TEST_F(remote_listener_test, a_listener_made_before_the_broadcaster_hears_it_once_it_comes)
{
    const dross::ipc::unix_socket place{ dross::path{ at("s") } };
    listening one{ place };

    auto news = dross::broadcaster::create(place);
    ASSERT_TRUE(news.has_value());

    EXPECT_TRUE(notify_until_heard(*news, *one.got, 42));
    EXPECT_TRUE(one.got->wait_for([&] {
        return one.got->links_of(dross::link_event::kind::connected) == 1;
    }));
}

TEST_F(remote_listener_test, connecting_is_reported_with_no_topic_and_no_reason)
{
    const dross::ipc::unix_socket place{ dross::path{ at("s") } };
    listening one{ place };
    auto news = dross::broadcaster::create(place);
    ASSERT_TRUE(news.has_value());

    ASSERT_TRUE(one.got->wait_for([&] {
        return ! one.got->links_held().empty();
    }));
    const auto first = one.got->links().front();
    EXPECT_EQ(first.what, dross::link_event::kind::connected);
    EXPECT_TRUE(first.topic_name.empty());
    EXPECT_FALSE(first.reason);
}

TEST_F(remote_listener_test, a_try_that_fails_is_not_reported)
{
    listening one{ dross::ipc::unix_socket{ dross::path{ at("s") } } };
    EXPECT_FALSE(one.got->wait_for([&] {
        return ! one.got->links_held().empty();
    }, 500ms));
}

TEST_F(remote_listener_test, the_listener_connects_again_once_the_broadcaster_comes_back)
{
    const dross::ipc::unix_socket place{ dross::path{ at("s") } };
    listening one{ place };
    {
        auto news = dross::broadcaster::create(place);
        ASSERT_TRUE(news.has_value());
        ASSERT_TRUE(notify_until_heard(*news, *one.got, 1));
    }
    ASSERT_TRUE(one.got->wait_for([&] {
        return one.got->links_of(dross::link_event::kind::disconnected) == 1;
    }));

    auto again = dross::broadcaster::create(place);
    ASSERT_TRUE(again.has_value());
    EXPECT_TRUE(notify_until_heard(*again, *one.got, 2));
    EXPECT_EQ(one.got->links().back().what, dross::link_event::kind::connected);
}

TEST_F(remote_listener_test, a_notification_sent_before_the_subscription_arrives_does_not_reach_it)
{
    const dross::ipc::unix_socket place{ dross::path{ at("s") } };
    auto news = dross::broadcaster::create(place);
    ASSERT_TRUE(news.has_value());
    news->notify(kCount, 1);

    listening one{ place };
    ASSERT_TRUE(notify_until_heard(*news, *one.got, 2));
    for (const auto each : one.got->counts()) {
        EXPECT_EQ(each, 2);
    }
}

TEST_F(remote_listener_test, a_value_the_type_cannot_be_made_from_is_reported_and_dropped)
{
    const dross::ipc::unix_socket place{ dross::path{ at("s") } };
    auto news = dross::broadcaster::create(place);
    ASSERT_TRUE(news.has_value());
    listening one{ place };
    ASSERT_TRUE(notify_until_heard(*news, *one.got, 1));

    news->notify(kCountAsText, std::string{ "one" });

    ASSERT_TRUE(one.got->wait_for([&] {
        return one.got->links_of(dross::link_event::kind::type_mismatch) == 1;
    }));
    const auto reported = one.got->links().back();
    EXPECT_EQ(reported.topic_name, kCount.name());
    EXPECT_TRUE(reported.reason);
    // Only the retried 1 was handed on, never the text.
    for (const auto each : one.got->counts()) {
        EXPECT_EQ(each, 1);
    }
}

TEST_F(remote_listener_test, subscribing_with_a_type_without_value_traits_fails)
{
    auto ears = dross::listener::create(dross::ipc::unix_socket{ dross::path{ at("s") } });
    const auto subscribed = ears.subscribe(kNoTraits, [](const no_traits&) {
    }, dross::delivery::direct());
    ASSERT_FALSE(subscribed.has_value());
    EXPECT_EQ(subscribed.error(), dross::error(dross::notification_errc::type_not_convertible));
}

TEST_F(remote_listener_test, a_listener_in_this_process_takes_a_type_without_value_traits)
{
    auto ears = dross::listener::create(dross::broadcaster::create());
    EXPECT_TRUE(ears.subscribe(kNoTraits, [](const no_traits&) {
    }, dross::delivery::direct())
                    .has_value());
}

TEST_F(remote_listener_test, the_broadcaster_is_told_each_name_once_and_when_it_is_let_go)
{
    fake_broadcaster there{ at("s") };
    auto ears = dross::listener::create(dross::ipc::unix_socket{ dross::path{ at("s") } });
    auto first = ears.subscribe(kCount, [](std::int64_t) {
    }, dross::delivery::direct());
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(there.wait_for([&] {
        return there.joined_held() && ! there.messages_held().empty();
    }));

    auto second = ears.subscribe(kCount, [](std::int64_t) {
    }, dross::delivery::direct());
    ASSERT_TRUE(second.has_value());
    first->reset();
    auto other = ears.subscribe(kOther, [](std::int64_t) {
    }, dross::delivery::direct());
    ASSERT_TRUE(other.has_value());
    second->reset();
    other->reset();

    const std::vector<std::string> expected{ "+com.example.test.count", "+com.example.test.other", "-com.example.test.count", "-com.example.test.other" };
    EXPECT_TRUE(there.wait_for([&] {
        return names_asked(there.messages_held()) == expected;
    }));
}

TEST_F(remote_listener_test, the_names_subscribed_are_asked_for_again_on_each_connection)
{
    auto ears = dross::listener::create(dross::ipc::unix_socket{ dross::path{ at("s") } });
    auto subscribed = ears.subscribe(kCount, [](std::int64_t) {
    }, dross::delivery::direct());
    ASSERT_TRUE(subscribed.has_value());

    const std::vector<std::string> expected{ "+com.example.test.count" };
    {
        fake_broadcaster there{ at("s") };
        EXPECT_TRUE(there.wait_for([&] {
            return names_asked(there.messages_held()) == expected;
        }));
    }
    fake_broadcaster again{ at("s") };
    EXPECT_TRUE(again.wait_for([&] {
        return names_asked(again.messages_held()) == expected;
    }));
}

TEST_F(remote_listener_test, a_peer_that_sends_a_subscription_is_disconnected_with_the_reason)
{
    fake_broadcaster there{ at("s") };
    listening one{ dross::ipc::unix_socket{ dross::path{ at("s") } } };
    ASSERT_TRUE(there.wait_for([&] {
        return there.joined_held();
    }));

    there.send(dross::wire::subscribe{ "com.example.test.count" });

    ASSERT_TRUE(one.got->wait_for([&] {
        return one.got->links_of(dross::link_event::kind::disconnected) >= 1;
    }));
    for (const auto& each : one.got->links()) {
        if (each.what == dross::link_event::kind::disconnected) {
            EXPECT_EQ(each.reason, dross::error(std::errc::invalid_argument));
            break;
        }
    }
}

TEST_F(remote_listener_test, nothing_is_reported_when_no_one_subscribes_to_link)
{
    const dross::ipc::unix_socket place{ dross::path{ at("s") } };
    auto news = dross::broadcaster::create(place);
    ASSERT_TRUE(news.has_value());
    auto got = std::make_shared<heard>();
    auto ears = dross::listener::create(place);
    auto subscribed = ears.subscribe(kCount, [got](const std::int64_t n) {
        got->count(n);
    }, dross::delivery::direct());
    ASSERT_TRUE(subscribed.has_value());

    EXPECT_TRUE(notify_until_heard(*news, *got, 1));
    EXPECT_TRUE(got->links().empty());
}

// Lets the last handle and subscription go from inside a subscriber's
// function, which runs on the listener's receiving thread.
TEST_F(remote_listener_test, the_listener_may_be_let_go_from_inside_a_direct_subscriber)
{
    const dross::ipc::unix_socket place{ dross::path{ at("s") } };

    std::mutex lock;
    std::condition_variable changed;
    std::optional<dross::subscription> held;
    std::expected<dross::broadcaster, dross::error> news = std::unexpected(dross::error{});
    bool gone = false;
    {
        auto ears = dross::listener::create(place);
        auto subscribed = ears.subscribe(dross::listener::link, [&](const dross::link_event&) {
            std::optional<dross::subscription> ending;
            {
                std::unique_lock guard{ lock };
                changed.wait(guard, [&] {
                    return held.has_value();
                });
                ending = std::move(held);
                held.reset();
            }
            ending.reset();
            const std::lock_guard guard{ lock };
            gone = true;
            changed.notify_all();
        }, dross::delivery::direct());
        ASSERT_TRUE(subscribed.has_value());
        {
            const std::lock_guard guard{ lock };
            held = std::move(*subscribed);
            changed.notify_all();
        }
        // Made after the subscription, which hears only what follows it.
        news = dross::broadcaster::create(place);
        ASSERT_TRUE(news.has_value());
    }

    std::unique_lock guard{ lock };
    EXPECT_TRUE(changed.wait_for(guard, 5s, [&] {
        return gone;
    }));
}

}  // namespace
