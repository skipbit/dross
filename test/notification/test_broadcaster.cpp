#include "../thread/test_support.h"
#include "dross/notification/broadcaster.h"
#include "dross/notification/delivery.h"
#include "dross/notification/listener.h"
#include "dross/notification/notification.h"
#include "dross/notification/subscription.h"
#include "dross/notification/topic.h"
#include "dross/thread/runloop.h"
#include "dross/thread/thread.h"

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using dross_test::event;

struct progress {
    int done;
    int total;
};

struct ratio {
    double value;
};

inline constexpr dross::topic<progress> kProgress{ "com.example.test.progress" };
inline constexpr dross::topic<ratio> kProgressAsRatio{ "com.example.test.progress" };
inline constexpr dross::topic<progress> kOther{ "com.example.test.other" };
inline constexpr dross::topic<int> kCount{ "com.example.test.count" };

// Counts the copies and moves made of it, through counters it shares.
struct counted final {
    std::shared_ptr<std::atomic<int>> copies;
    std::shared_ptr<std::atomic<int>> moves;

    counted()
        : copies{ std::make_shared<std::atomic<int>>(0) }
        , moves{ std::make_shared<std::atomic<int>>(0) }
    {
    }

    counted(const counted& other)
        : copies{ other.copies }
        , moves{ other.moves }
    {
        ++*copies;
    }

    counted(counted&& other) noexcept
        : copies{ other.copies }
        , moves{ other.moves }
    {
        ++*moves;
    }
};

inline constexpr dross::topic<counted> kCounted{ "com.example.test.counted" };

// A type with a member function, for a pointer to member as a subscriber.
struct has_member final {
    void show() const
    {
    }
};

inline constexpr dross::topic<has_member> kHasMember{ "com.example.test.has_member" };

// Subscribes and unwraps, failing the test when subscribing fails.
template <typename T, typename F>
dross::subscription subscribe(dross::listener& ears, const dross::topic<T>& which, F&& function, const dross::delivery& how)
{
    auto made = ears.subscribe(which, std::forward<F>(function), how);
    EXPECT_TRUE(made.has_value());
    return made ? std::move(*made) : dross::subscription{};
}

}  // namespace

TEST(broadcaster_test, handles_name_the_broadcaster_they_were_copied_from)
{
    const auto first = dross::broadcaster::create();
    const auto copy = first;
    const auto second = dross::broadcaster::create();

    EXPECT_TRUE(copy == first);
    EXPECT_FALSE(second == first);
}

TEST(broadcaster_test, listener_handles_name_the_listener_they_were_copied_from)
{
    const auto news = dross::broadcaster::create();
    const auto first = dross::listener::create(news);
    const auto copy = first;
    const auto second = dross::listener::create(news);

    EXPECT_TRUE(copy == first);
    EXPECT_FALSE(second == first);
}

TEST(broadcaster_test, a_subscriber_is_called_with_the_value_sent)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::vector<int> seen;
    auto token = subscribe(ears, kProgress, [&seen](const progress& now) {
        seen.push_back(now.done);
        seen.push_back(now.total);
    }, dross::delivery::direct());

    news.notify(kProgress, progress{ 3, 10 });

    EXPECT_EQ(seen, (std::vector<int>{ 3, 10 }));
}

TEST(broadcaster_test, every_subscriber_is_called_with_the_same_one_value)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::vector<const progress*> seen;
    auto record = [&seen](const progress& now) {
        seen.push_back(&now);
    };
    auto direct = subscribe(ears, kProgress, record, dross::delivery::direct());
    auto queued = subscribe(ears, kProgress, record, dross::delivery::queued());

    news.notify(kProgress, progress{ 1, 2 });
    dross::current_runloop().run_pending();

    ASSERT_EQ(seen.size(), 2U);
    EXPECT_EQ(seen[0], seen[1]);
}

TEST(broadcaster_test, a_value_sent_is_copied_once_from_an_lvalue_and_never_from_an_rvalue)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    auto first = subscribe(ears, kCounted, [](const counted&) {
    }, dross::delivery::direct());
    auto second = subscribe(ears, kCounted, [](const counted&) {
    }, dross::delivery::direct());

    const counted kept;
    news.notify(kCounted, kept);
    EXPECT_EQ(kept.copies->load(), 1);

    counted moved;
    const auto copies = moved.copies;
    news.notify(kCounted, std::move(moved));
    EXPECT_EQ(copies->load(), 0);
}

TEST(broadcaster_test, a_notification_no_one_subscribes_to_is_not_put_anywhere)
{
    auto news = dross::broadcaster::create();

    counted moved;
    const auto moves = moved.moves;
    const auto copies = moved.copies;
    news.notify(kCounted, std::move(moved));

    // One move into the parameter, and none into a value for subscribers.
    EXPECT_EQ(moves->load(), 1);
    EXPECT_EQ(copies->load(), 0);
}

TEST(broadcaster_test, topics_of_one_name_and_different_types_do_not_reach_each_other)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::vector<std::string> seen;
    auto as_progress = subscribe(ears, kProgress, [&seen](const progress&) {
        seen.push_back("progress");
    }, dross::delivery::direct());
    auto as_ratio = subscribe(ears, kProgressAsRatio, [&seen](const ratio&) {
        seen.push_back("ratio");
    }, dross::delivery::direct());

    news.notify(kProgressAsRatio, ratio{ 0.5 });
    news.notify(kProgress, progress{ 1, 2 });

    EXPECT_EQ(seen, (std::vector<std::string>{ "ratio", "progress" }));
}

TEST(broadcaster_test, topics_of_different_names_do_not_reach_each_other)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    int called = 0;
    auto token = subscribe(ears, kProgress, [&called](const progress&) {
        ++called;
    }, dross::delivery::direct());

    news.notify(kOther, progress{ 1, 2 });

    EXPECT_EQ(called, 0);
}

TEST(broadcaster_test, a_notification_sent_before_subscribing_is_not_received)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    news.notify(kProgress, progress{ 1, 2 });

    int called = 0;
    auto token = subscribe(ears, kProgress, [&called](const progress&) {
        ++called;
    }, dross::delivery::direct());

    EXPECT_EQ(called, 0);
}

TEST(broadcaster_test, notifications_from_one_thread_are_received_in_the_order_sent)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::vector<int> seen;
    auto token = subscribe(ears, kCount, [&seen](const int& value) {
        seen.push_back(value);
    }, dross::delivery::queued());

    for (int value = 0; value < 5; ++value) {
        news.notify(kCount, value);
    }
    dross::current_runloop().run_pending();

    EXPECT_EQ(seen, (std::vector<int>{ 0, 1, 2, 3, 4 }));
}

TEST(broadcaster_test, one_who_subscribes_from_inside_a_subscriber_receives_the_next_notification)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::vector<int> late;
    std::optional<dross::subscription> later;
    auto first = subscribe(ears, kCount, [&](const int&) {
        if (! later) {
            later = subscribe(ears, kCount, [&late](const int& value) {
                late.push_back(value);
            }, dross::delivery::direct());
        }
    }, dross::delivery::direct());

    news.notify(kCount, 1);
    news.notify(kCount, 2);

    EXPECT_EQ(late, std::vector<int>{ 2 });
}

TEST(broadcaster_test, a_subscriber_can_send_from_inside_its_function)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::vector<int> seen;
    auto token = subscribe(ears, kCount, [&](const int& value) {
        seen.push_back(value);
        if (value < 3) {
            news.notify(kCount, value + 1);
        }
    }, dross::delivery::direct());

    news.notify(kCount, 1);

    EXPECT_EQ(seen, (std::vector<int>{ 1, 2, 3 }));
}

TEST(broadcaster_test, nothing_sent_after_a_subscription_ends_is_received)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    int called = 0;
    auto token = subscribe(ears, kCount, [&called](const int&) {
        ++called;
    }, dross::delivery::direct());

    token.reset();
    news.notify(kCount, 1);

    EXPECT_EQ(called, 0);
}

TEST(broadcaster_test, an_ended_subscribers_function_is_let_go_when_it_ends)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    auto held = std::make_shared<int>(0);
    const std::weak_ptr<int> watch = held;
    auto token = subscribe(ears, kCount, [held = std::move(held)](const int&) {
    }, dross::delivery::direct());

    token.reset();

    EXPECT_TRUE(watch.expired());
}

TEST(broadcaster_test, a_subscriber_that_holds_its_broadcaster_is_let_go_when_it_ends)
{
    std::optional<dross::broadcaster> news{ dross::broadcaster::create() };
    std::optional<dross::listener> ears{ dross::listener::create(*news) };
    auto held = std::make_shared<int>(0);
    const std::weak_ptr<int> watch = held;
    auto token = subscribe(*ears, kCount, [handle = *news, held = std::move(held)](const int&) {
    }, dross::delivery::direct());

    token.reset();
    news.reset();
    ears.reset();

    EXPECT_TRUE(watch.expired());
}

TEST(broadcaster_test, a_subscription_goes_on_after_every_listener_handle_has_gone)
{
    auto news = dross::broadcaster::create();
    int called = 0;
    dross::subscription token;
    {
        auto ears = dross::listener::create(news);
        token = subscribe(ears, kCount, [&called](const int&) {
            ++called;
        }, dross::delivery::direct());
    }

    news.notify(kCount, 1);

    EXPECT_EQ(called, 1);
    EXPECT_TRUE(token.active());
}

TEST(broadcaster_test, a_subscription_is_not_active_once_its_broadcaster_has_gone)
{
    std::optional<dross::broadcaster> news{ dross::broadcaster::create() };
    auto ears = dross::listener::create(*news);
    auto token = subscribe(ears, kCount, [](const int&) {
    }, dross::delivery::direct());
    EXPECT_TRUE(token.active());

    news.reset();

    EXPECT_FALSE(token.active());
}

TEST(broadcaster_test, subscribing_after_the_broadcaster_has_gone_makes_a_subscription_that_is_not_active)
{
    std::optional<dross::broadcaster> news{ dross::broadcaster::create() };
    auto ears = dross::listener::create(*news);
    news.reset();

    auto made = ears.subscribe(kCount, [](const int&) {
    }, dross::delivery::direct());

    ASSERT_TRUE(made.has_value());
    EXPECT_FALSE(made->active());
}

TEST(broadcaster_test, subscribing_with_an_empty_function_is_an_error)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    void (*no_function)(const int&) = nullptr;
    const std::function<void(const int&)> empty;

    auto from_pointer = ears.subscribe(kCount, no_function, dross::delivery::direct());
    auto from_function = ears.subscribe(kCount, empty, dross::delivery::direct());

    ASSERT_FALSE(from_pointer.has_value());
    EXPECT_EQ(from_pointer.error(), dross::notification_errc::empty_function);
    ASSERT_FALSE(from_function.has_value());
    EXPECT_EQ(from_function.error(), dross::notification_errc::empty_function);
}

TEST(broadcaster_test, subscribing_with_an_empty_member_pointer_is_an_error)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    void (has_member::*no_member)() const = nullptr;

    auto made = ears.subscribe(kHasMember, no_member, dross::delivery::direct());

    ASSERT_FALSE(made.has_value());
    EXPECT_EQ(made.error(), dross::notification_errc::empty_function);
}

TEST(broadcaster_test, the_error_category_is_named_and_describes_its_values)
{
    const std::error_code code = dross::notification_errc::empty_function;

    EXPECT_STREQ(code.category().name(), "dross.notification");
    EXPECT_FALSE(code.message().empty());
}

TEST(broadcaster_test, an_exception_from_a_direct_subscriber_propagates_out_of_notify_and_stops_the_rest)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    int after = 0;
    auto throwing = subscribe(ears, kCount, [](const int&) {
        throw std::runtime_error{ "from the subscriber" };
    }, dross::delivery::direct());
    auto later = subscribe(ears, kCount, [&after](const int&) {
        ++after;
    }, dross::delivery::direct());

    EXPECT_THROW(news.notify(kCount, 1), std::runtime_error);
    EXPECT_EQ(after, 0);
}

TEST(broadcaster_test, a_subscriber_on_another_thread_is_called_there)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    dross::thread worker;
    auto known = std::make_shared<event>();
    dross::runloop worker_loop = dross::current_runloop();
    std::thread::id worker_id;
    worker.perform([&, known]() {
        worker_loop = dross::current_runloop();
        worker_id = std::this_thread::get_id();
        known->set();
    });
    ASSERT_TRUE(known->wait());

    auto called = std::make_shared<event>();
    std::thread::id called_on;
    auto token = subscribe(ears, kCount, [&called_on, called](const int&) {
        called_on = std::this_thread::get_id();
        called->set();
    }, dross::delivery::queued(worker_loop));
    news.notify(kCount, 1);

    ASSERT_TRUE(called->wait());
    EXPECT_EQ(called_on, worker_id);
    token.reset();
    worker.quit();
    worker.join();
}

TEST(broadcaster_test, sending_subscribing_and_ending_on_many_threads_at_once_is_free_of_races)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::atomic<int> received{ 0 };
    auto always = subscribe(ears, kCount, [&received](const int&) {
        ++received;
    }, dross::delivery::direct());

    constexpr int kThreads = 4;
    constexpr int kRounds = 200;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&]() {
            for (int round = 0; round < kRounds; ++round) {
                auto copy = ears;
                auto brief = subscribe(copy, kCount, [](const int&) {
                }, dross::delivery::direct());
                news.notify(kCount, round);
                brief.reset();
            }
        });
    }
    for (auto& each : threads) {
        each.join();
    }

    EXPECT_EQ(received.load(), kThreads * kRounds);
}

TEST(broadcaster_test, notify_returns_a_notification_that_can_be_kept)
{
    auto news = dross::broadcaster::create();

    const dross::notification sent = news.notify(kCount, 1);
    const dross::notification copy = sent;

    EXPECT_TRUE(std::is_copy_constructible_v<dross::notification>);
}
