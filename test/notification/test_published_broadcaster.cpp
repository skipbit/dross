#include "dross/ipc/unix_socket.h"
#include "dross/notification/broadcaster.h"
#include "dross/notification/delivery.h"
#include "dross/notification/listener.h"
#include "dross/notification/topic.h"
#include "dross/type/value_traits.h"
#include "ipc/socket_directory.h"
#include "ipc/unix_transport.h"
#include "notification/wire.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <variant>
#include <vector>

namespace {

using namespace std::chrono_literals;

using published_broadcaster_test = dross_test::socket_directory_test;

inline constexpr dross::topic<std::int64_t> kCount{ "com.example.test.count" };
inline constexpr dross::topic<std::int64_t> kOther{ "com.example.test.other" };

struct no_traits {
    int n;
};

inline constexpr dross::topic<no_traits> kNoTraits{ "com.example.test.no_traits" };

// Stands in for a listener in another process: connects to the
// broadcaster's socket, and keeps what it is sent.
class remote final : public dross::transport_events {
public:
    explicit remote(const std::string& path)
    {
        auto made = dross::unix_transport::connect(dross::path{ path }, *this);
        if (made) {
            _transport = std::move(*made);
            _transport->start();
        }
    }

    ~remote() override
    {
        _transport.reset();
    }

    bool ready()
    {
        return wait_for([this] {
            return _id.has_value();
        });
    }

    void send(const dross::wire::message& which)
    {
        const auto id = connection();
        ASSERT_TRUE(id.has_value());
        ASSERT_TRUE(_transport->send(*id, dross::wire::encode(which)).has_value());
    }

    // The notices received so far, in order.
    std::vector<dross::wire::notice> notices()
    {
        const std::lock_guard guard{ _lock };
        return _notices;
    }

    // The same, for a condition given to wait_for(), which holds the lock.
    const std::vector<dross::wire::notice>& notices_held() const
    {
        return _notices;
    }

    bool closed()
    {
        return wait_for([this] {
            return _closed;
        });
    }

    bool wait_for(const std::function<bool()>& done, const std::chrono::milliseconds limit = 5s)
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, limit, done);
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
        if (read && std::holds_alternative<dross::wire::notice>(*read)) {
            _notices.push_back(std::get<dross::wire::notice>(std::move(*read)));
        }
        _changed.notify_all();
    }

    void closed(dross::connection_id, dross::error) override
    {
        const std::lock_guard guard{ _lock };
        _closed = true;
        _changed.notify_all();
    }

    void stopped(dross::error) override
    {
    }

private:
    std::optional<dross::connection_id> connection()
    {
        const std::lock_guard guard{ _lock };
        return _id;
    }

    std::mutex _lock;
    std::condition_variable _changed;
    std::unique_ptr<dross::unix_transport> _transport;
    std::optional<dross::connection_id> _id;
    std::vector<dross::wire::notice> _notices;
    bool _closed{ false };
};

std::int64_t count_of(const dross::wire::notice& sent)
{
    return dross::value_traits<std::int64_t>::from_value(sent.content).value_or(-1);
}

// Notifies value on which until peer has received a notice on it, since a
// subscription takes effect only once it reaches the broadcaster.
bool notify_until_received(dross::broadcaster& news, remote& peer, const dross::topic<std::int64_t>& which, const std::int64_t value)
{
    for (int i = 0; i < 500; ++i) {
        news.notify(which, value);
        const bool got = peer.wait_for([&] {
            for (const auto& each : peer.notices_held()) {
                if (each.name == which.name()) {
                    return true;
                }
            }
            return false;
        }, 10ms);
        if (got) {
            return true;
        }
    }
    return false;
}

TEST_F(published_broadcaster_test, create_fails_where_it_cannot_listen)
{
    const auto made = dross::broadcaster::create(dross::ipc::unix_socket{ dross::path{ at("missing/s") } });
    ASSERT_FALSE(made.has_value());
    EXPECT_EQ(made.error(), dross::error(std::errc::no_such_file_or_directory));
}

TEST_F(published_broadcaster_test, create_fails_where_another_broadcaster_listens)
{
    const dross::ipc::unix_socket place{ dross::path{ at("s") } };
    const auto first = dross::broadcaster::create(place);
    ASSERT_TRUE(first.has_value());
    const auto second = dross::broadcaster::create(place);
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), dross::error(std::errc::address_in_use));
}

TEST_F(published_broadcaster_test, a_subscribed_listener_receives_what_is_notified_on_its_name)
{
    auto news = dross::broadcaster::create(dross::ipc::unix_socket{ dross::path{ at("s") } });
    ASSERT_TRUE(news.has_value());
    remote peer{ at("s") };
    ASSERT_TRUE(peer.ready());
    peer.send(dross::wire::subscribe{ std::string{ kCount.name() } });

    ASSERT_TRUE(notify_until_received(*news, peer, kCount, 42));
    const auto got = peer.notices();
    EXPECT_EQ(got.back().name, kCount.name());
    EXPECT_EQ(count_of(got.back()), 42);
}

TEST_F(published_broadcaster_test, nothing_is_sent_on_a_name_not_subscribed_to)
{
    auto news = dross::broadcaster::create(dross::ipc::unix_socket{ dross::path{ at("s") } });
    ASSERT_TRUE(news.has_value());
    remote peer{ at("s") };
    ASSERT_TRUE(peer.ready());
    peer.send(dross::wire::subscribe{ std::string{ kCount.name() } });
    ASSERT_TRUE(notify_until_received(*news, peer, kCount, 1));

    news->notify(kOther, 2);
    news->notify(kCount, 3);
    ASSERT_TRUE(peer.wait_for([&] {
        return count_of(peer.notices_held().back()) == 3;
    }));
    for (const auto& each : peer.notices()) {
        EXPECT_EQ(each.name, kCount.name());
    }
}

TEST_F(published_broadcaster_test, each_listener_receives_only_the_names_it_subscribed_to)
{
    auto news = dross::broadcaster::create(dross::ipc::unix_socket{ dross::path{ at("s") } });
    ASSERT_TRUE(news.has_value());
    remote counting{ at("s") };
    remote other{ at("s") };
    ASSERT_TRUE(counting.ready());
    ASSERT_TRUE(other.ready());
    counting.send(dross::wire::subscribe{ std::string{ kCount.name() } });
    other.send(dross::wire::subscribe{ std::string{ kOther.name() } });
    ASSERT_TRUE(notify_until_received(*news, counting, kCount, 1));
    ASSERT_TRUE(notify_until_received(*news, other, kOther, 1));

    news->notify(kOther, 2);
    news->notify(kCount, 3);
    ASSERT_TRUE(counting.wait_for([&] {
        return count_of(counting.notices_held().back()) == 3;
    }));
    for (const auto& each : counting.notices()) {
        EXPECT_EQ(each.name, kCount.name());
    }
}

TEST_F(published_broadcaster_test, nothing_more_is_sent_after_unsubscribing)
{
    auto news = dross::broadcaster::create(dross::ipc::unix_socket{ dross::path{ at("s") } });
    ASSERT_TRUE(news.has_value());
    remote peer{ at("s") };
    ASSERT_TRUE(peer.ready());
    peer.send(dross::wire::subscribe{ std::string{ kCount.name() } });
    ASSERT_TRUE(notify_until_received(*news, peer, kCount, 1));

    // Messages from one peer are read in order, so once the second name
    // arrives the first is no longer subscribed.
    peer.send(dross::wire::unsubscribe{ std::string{ kCount.name() } });
    peer.send(dross::wire::subscribe{ std::string{ kOther.name() } });
    ASSERT_TRUE(notify_until_received(*news, peer, kOther, 1));
    const auto before = peer.notices().size();

    news->notify(kCount, 2);
    news->notify(kOther, 3);
    ASSERT_TRUE(peer.wait_for([&] {
        return count_of(peer.notices_held().back()) == 3;
    }));
    // A retried notice of the second name may still arrive late; none of the
    // first may.
    const auto got = peer.notices();
    for (auto each = got.begin() + static_cast<std::ptrdiff_t>(before); each != got.end(); ++each) {
        EXPECT_EQ(each->name, kOther.name());
    }
}

TEST_F(published_broadcaster_test, a_listener_in_this_process_hears_it_directly)
{
    auto news = dross::broadcaster::create(dross::ipc::unix_socket{ dross::path{ at("s") } });
    ASSERT_TRUE(news.has_value());
    auto ears = dross::listener::create(*news);
    std::int64_t heard = 0;
    auto subscribed = ears.subscribe(kCount, [&](const std::int64_t n) {
        heard = n;
    }, dross::delivery::direct());
    ASSERT_TRUE(subscribed.has_value());

    news->notify(kCount, 7);
    EXPECT_EQ(heard, 7);
}

TEST_F(published_broadcaster_test, a_peer_that_sends_a_notice_is_disconnected)
{
    auto news = dross::broadcaster::create(dross::ipc::unix_socket{ dross::path{ at("s") } });
    ASSERT_TRUE(news.has_value());
    remote peer{ at("s") };
    ASSERT_TRUE(peer.ready());
    peer.send(dross::wire::notice{ std::string{ kCount.name() }, dross::value_traits<std::int64_t>::to_value(1) });
    EXPECT_TRUE(peer.closed());
}

TEST_F(published_broadcaster_test, the_socket_file_goes_with_the_last_handle)
{
    {
        auto news = dross::broadcaster::create(dross::ipc::unix_socket{ dross::path{ at("s") } });
        ASSERT_TRUE(news.has_value());
        const dross::broadcaster copy = *news;
        EXPECT_TRUE(std::filesystem::exists(std::filesystem::symlink_status(at("s"))));
    }
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::symlink_status(at("s"))));
}

TEST_F(published_broadcaster_test, the_same_place_can_be_published_again_once_the_last_handle_goes)
{
    const dross::ipc::unix_socket place{ dross::path{ at("s") } };
    {
        const auto first = dross::broadcaster::create(place);
        ASSERT_TRUE(first.has_value());
    }
    EXPECT_TRUE(dross::broadcaster::create(place).has_value());
}

using published_broadcaster_death_test = published_broadcaster_test;

TEST_F(published_broadcaster_death_test, sending_a_type_without_value_traits_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    const std::string path = at("s");
    EXPECT_DEATH({
        auto news = dross::broadcaster::create(dross::ipc::unix_socket{ dross::path{ path } });
        if (news) {
            news->notify(kNoTraits, no_traits{ 1 });
        }
    },
                 "broadcaster::notify: a published broadcaster sends a type without dross::value_traits");
}

}  // namespace
