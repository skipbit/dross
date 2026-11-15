#include "../thread/test_support.h"
#include "dross/notification/broadcaster.h"
#include "dross/notification/delivery.h"
#include "dross/notification/listener.h"
#include "dross/notification/notification.h"
#include "dross/notification/subscription.h"
#include "dross/notification/topic.h"
#include "dross/thread/operation_queue.h"
#include "dross/thread/runloop.h"
#include "dross/thread/thread.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using dross_test::event;
using dross_test::kTimeout;

inline constexpr dross::topic<int> kCount{ "com.example.test.count" };
inline constexpr dross::topic<std::shared_ptr<int>> kHeld{ "com.example.test.held" };

const dross::notify_options kDeferred{ .timing = dross::notify_timing::deferred };

constexpr auto kTimeoutMs = std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout);

// Subscribes and unwraps, failing the test when subscribing fails.
template <typename T, typename F>
dross::subscription subscribe(dross::listener& ears, const dross::topic<T>& which, F&& function, const dross::delivery& how)
{
    auto made = ears.subscribe(which, std::forward<F>(function), how);
    EXPECT_TRUE(made.has_value());
    return made ? std::move(*made) : dross::subscription{};
}

}  // namespace

TEST(notification_test, a_deferred_direct_subscriber_runs_only_after_notify_returns)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::vector<int> seen;
    auto token = subscribe(ears, kCount, [&seen](const int& value) {
        seen.push_back(value);
    }, dross::delivery::direct());

    news.notify(kCount, 1, kDeferred);
    EXPECT_TRUE(seen.empty());
    dross::current_runloop().run_pending();

    EXPECT_EQ(seen, (std::vector<int>{ 1 }));
}

TEST(notification_test, deferred_direct_subscribers_run_in_the_order_they_subscribed)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::vector<std::string> seen;
    auto first = subscribe(ears, kCount, [&seen](const int& value) {
        seen.push_back("first " + std::to_string(value));
    }, dross::delivery::direct());
    auto second = subscribe(ears, kCount, [&seen](const int& value) {
        seen.push_back("second " + std::to_string(value));
    }, dross::delivery::direct());

    news.notify(kCount, 1, kDeferred);
    news.notify(kCount, 2, kDeferred);
    dross::current_runloop().run_pending();

    EXPECT_EQ(seen, (std::vector<std::string>{ "first 1", "second 1", "first 2", "second 2" }));
}

TEST(notification_test, a_deferred_notification_reaches_a_direct_subscriber_after_one_sent_later_without_deferring)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::vector<int> seen;
    auto token = subscribe(ears, kCount, [&seen](const int& value) {
        seen.push_back(value);
    }, dross::delivery::direct());

    news.notify(kCount, 1, kDeferred);
    news.notify(kCount, 2);
    dross::current_runloop().run_pending();

    EXPECT_EQ(seen, (std::vector<int>{ 2, 1 }));
}

TEST(notification_test, an_exception_from_a_deferred_direct_subscriber_propagates_out_of_the_running_loop)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    auto token = subscribe(ears, kCount, [](const int&) {
        throw std::runtime_error{ "from the subscriber" };
    }, dross::delivery::direct());

    EXPECT_NO_THROW(news.notify(kCount, 1, kDeferred));
    EXPECT_THROW(dross::current_runloop().run_pending(), std::runtime_error);
}

TEST(notification_test, deferring_leaves_a_queued_subscriber_called_once)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    int calls = 0;
    auto token = subscribe(ears, kCount, [&calls](const int&) {
        ++calls;
    }, dross::delivery::queued());

    news.notify(kCount, 1, kDeferred);
    EXPECT_EQ(calls, 0);
    dross::current_runloop().run_pending();
    dross::current_runloop().run_pending();

    EXPECT_EQ(calls, 1);
}

TEST(notification_test, a_deferred_delivery_goes_with_a_thread_that_never_runs_its_loop)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    auto calls = std::make_shared<std::atomic<int>>(0);
    auto token = subscribe(ears, kHeld, [calls](const std::shared_ptr<int>&) {
        ++*calls;
    }, dross::delivery::direct());
    auto held = std::make_shared<int>(0);
    const std::weak_ptr<int> watch = held;

    dross::thread once{ [news, held = std::move(held)]() mutable {
        news.notify(kHeld, std::move(held), kDeferred);
    } };
    ASSERT_TRUE(once.join_for(kTimeoutMs));

    EXPECT_EQ(calls->load(), 0);
    EXPECT_TRUE(watch.expired());
}

TEST(notification_test, cancel_stops_a_delivery_queued_on_a_run_loop)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    int calls = 0;
    auto token = subscribe(ears, kCount, [&calls](const int&) {
        ++calls;
    }, dross::delivery::queued());

    auto sent = news.notify(kCount, 1);
    sent.cancel();
    dross::current_runloop().run_pending();

    EXPECT_EQ(calls, 0);
}

TEST(notification_test, cancel_stops_a_deferred_direct_delivery)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    int calls = 0;
    auto token = subscribe(ears, kCount, [&calls](const int&) {
        ++calls;
    }, dross::delivery::direct());

    auto sent = news.notify(kCount, 1, kDeferred);
    sent.cancel();
    dross::current_runloop().run_pending();

    EXPECT_EQ(calls, 0);
}

TEST(notification_test, cancel_stops_a_delivery_queued_on_an_operation_queue)
{
    dross::operation_queue workers{ 1 };
    auto release = std::make_shared<event>();
    auto blocking = std::make_shared<event>();
    workers.submit([release, blocking]() {
        blocking->set();
        release->wait();
    });
    ASSERT_TRUE(blocking->wait());

    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    auto calls = std::make_shared<std::atomic<int>>(0);
    auto token = subscribe(ears, kCount, [calls](const int&) {
        ++*calls;
    }, dross::delivery::on(workers));

    auto sent = news.notify(kCount, 1);
    sent.cancel();
    release->set();
    ASSERT_TRUE(workers.wait_for(kTimeoutMs));

    EXPECT_EQ(calls->load(), 0);
}

TEST(notification_test, cancel_stops_only_what_has_not_started)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    int direct_calls = 0;
    int queued_calls = 0;
    auto in_place = subscribe(ears, kCount, [&direct_calls](const int&) {
        ++direct_calls;
    }, dross::delivery::direct());
    auto later = subscribe(ears, kCount, [&queued_calls](const int&) {
        ++queued_calls;
    }, dross::delivery::queued());

    auto sent = news.notify(kCount, 1);
    sent.cancel();
    dross::current_runloop().run_pending();

    EXPECT_EQ(direct_calls, 1);
    EXPECT_EQ(queued_calls, 0);
}

TEST(notification_test, cancel_through_a_copy_stops_that_sending_and_no_other)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::vector<int> seen;
    auto token = subscribe(ears, kCount, [&seen](const int& value) {
        seen.push_back(value);
    }, dross::delivery::queued());

    const auto first = news.notify(kCount, 1);
    const auto second = news.notify(kCount, 2);
    auto copy = first;
    copy.cancel();
    dross::current_runloop().run_pending();

    EXPECT_EQ(seen, (std::vector<int>{ 2 }));
}

TEST(notification_test, a_subscriber_can_cancel_the_rest_of_its_sending_from_inside_its_function)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::optional<dross::notification> sent;
    std::vector<std::string> seen;
    auto first = subscribe(ears, kCount, [&seen, &sent](const int&) {
        seen.push_back("first");
        sent->cancel();
        sent->cancel();
    }, dross::delivery::queued());
    auto second = subscribe(ears, kCount, [&seen](const int&) {
        seen.push_back("second");
    }, dross::delivery::queued());

    sent.emplace(news.notify(kCount, 1));
    dross::current_runloop().run_pending();

    EXPECT_EQ(seen, (std::vector<std::string>{ "first" }));
}

TEST(notification_test, cancelling_leaves_the_subscription_receiving_later_notifications)
{
    dross_test::reset_main_runloop();
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    std::vector<int> seen;
    auto token = subscribe(ears, kCount, [&seen](const int& value) {
        seen.push_back(value);
    }, dross::delivery::queued());

    news.notify(kCount, 1).cancel();
    news.notify(kCount, 2);
    dross::current_runloop().run_pending();

    EXPECT_EQ(seen, (std::vector<int>{ 2 }));
    EXPECT_TRUE(token.active());
}

TEST(notification_test, cancelling_a_sending_no_one_subscribed_to_does_nothing)
{
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);

    auto sent = news.notify(kCount, 1);
    sent.cancel();
    sent.cancel();
    int calls = 0;
    auto token = subscribe(ears, kCount, [&calls](const int&) {
        ++calls;
    }, dross::delivery::direct());
    news.notify(kCount, 2);

    EXPECT_EQ(calls, 1);
}

TEST(notification_test, cancel_does_not_wait_for_a_call_running_on_another_thread)
{
    dross::operation_queue workers{ 1 };
    auto news = dross::broadcaster::create();
    auto ears = dross::listener::create(news);
    auto entered = std::make_shared<event>();
    auto release = std::make_shared<event>();
    auto token = subscribe(ears, kCount, [entered, release](const int&) {
        entered->set();
        release->wait();
    }, dross::delivery::on(workers));

    auto sent = news.notify(kCount, 1);
    ASSERT_TRUE(entered->wait());
    auto returned = std::make_shared<event>();
    std::thread canceller{ [sent, returned]() mutable {
        sent.cancel();
        returned->set();
    } };
    const bool without_waiting = returned->wait();
    release->set();
    canceller.join();
    ASSERT_TRUE(workers.wait_for(kTimeoutMs));

    EXPECT_TRUE(without_waiting);
}
