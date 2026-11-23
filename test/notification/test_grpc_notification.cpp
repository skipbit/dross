#include "dross/ipc/grpc/grpc_address.h"
#include "dross/ipc/grpc/grpc_target.h"
#include "dross/notification/broadcaster.h"
#include "dross/notification/delivery.h"
#include "dross/notification/link_event.h"
#include "dross/notification/listener.h"
#include "dross/notification/topic.h"
#include "ipc/grpc/free_port.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;

inline constexpr dross::topic<std::int64_t> kCount{ "com.example.test.count" };
inline constexpr dross::topic<std::int64_t> kOther{ "com.example.test.other" };

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
            _links.push_back(what.what);
        });
    }

    bool wait_for(const std::function<bool()>& done, const std::chrono::milliseconds limit = 10s)
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, limit, done);
    }

    // Read inside a condition given to wait_for(), which holds the lock.
    const std::vector<std::int64_t>& counts_held() const
    {
        return _counts;
    }

    std::size_t links_of(const dross::link_event::kind what) const
    {
        std::size_t found = 0;
        for (const auto each : _links) {
            found += (each == what) ? 1 : 0;
        }
        return found;
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
    std::vector<dross::link_event::kind> _links;
};

// A listener at a gRPC target with a subscriber on link and one on which,
// both direct.
struct listening final {
    listening(const dross::ipc::grpc_target& place, const dross::topic<std::int64_t>& which)
        : ears{ dross::listener::create(place).value() }
    {
        auto watched = ears.subscribe(dross::listener::link, [kept = got](const dross::link_event& what) {
            kept->link(what);
        }, dross::delivery::direct());
        auto subscribed = ears.subscribe(which, [kept = got](const std::int64_t n) {
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
bool notify_until_heard(dross::broadcaster& news, const dross::topic<std::int64_t>& which, heard& got, const std::int64_t value)
{
    for (int i = 0; i < 1000; ++i) {
        news.notify(which, value);
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

class grpc_notification_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        _address = dross_test::free_loopback_address();
        ASSERT_FALSE(_address.empty());
    }

    dross::ipc::grpc_address address() const
    {
        return { _address, grpc::InsecureServerCredentials() };
    }

    dross::ipc::grpc_target target() const
    {
        return { _address, grpc::InsecureChannelCredentials() };
    }

private:
    std::string _address;
};

TEST_F(grpc_notification_test, a_listener_hears_a_broadcaster_published_at_the_address)
{
    auto news = dross::broadcaster::create(address());
    ASSERT_TRUE(news.has_value());
    listening one{ target(), kCount };

    EXPECT_TRUE(notify_until_heard(*news, kCount, *one.got, 42));
    EXPECT_TRUE(one.got->wait_for([&] {
        return one.got->links_of(dross::link_event::kind::connected) == 1;
    }));
}

TEST_F(grpc_notification_test, a_listener_made_before_the_broadcaster_hears_it_once_it_comes)
{
    listening one{ target(), kCount };
    auto news = dross::broadcaster::create(address());
    ASSERT_TRUE(news.has_value());

    EXPECT_TRUE(notify_until_heard(*news, kCount, *one.got, 7));
}

TEST_F(grpc_notification_test, each_listener_hears_only_the_names_it_subscribed_to)
{
    auto news = dross::broadcaster::create(address());
    ASSERT_TRUE(news.has_value());
    listening counting{ target(), kCount };
    listening other{ target(), kOther };

    ASSERT_TRUE(notify_until_heard(*news, kCount, *counting.got, 1));
    ASSERT_TRUE(notify_until_heard(*news, kOther, *other.got, 2));
    EXPECT_TRUE(other.got->wait_for([&] {
        for (const auto each : other.got->counts_held()) {
            if (each != 2) {
                return false;
            }
        }
        return true;
    }));
}

TEST_F(grpc_notification_test, the_listener_connects_again_once_the_broadcaster_comes_back)
{
    listening one{ target(), kCount };
    {
        auto news = dross::broadcaster::create(address());
        ASSERT_TRUE(news.has_value());
        ASSERT_TRUE(notify_until_heard(*news, kCount, *one.got, 1));
    }
    ASSERT_TRUE(one.got->wait_for([&] {
        return one.got->links_of(dross::link_event::kind::disconnected) == 1;
    }));

    auto again = dross::broadcaster::create(address());
    ASSERT_TRUE(again.has_value());
    EXPECT_TRUE(notify_until_heard(*again, kCount, *one.got, 2));
}

TEST_F(grpc_notification_test, a_second_broadcaster_at_the_same_address_fails)
{
    auto news = dross::broadcaster::create(address());
    ASSERT_TRUE(news.has_value());
    const auto second = dross::broadcaster::create(address());
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), std::errc::address_not_available);
}

}  // namespace
