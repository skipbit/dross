#include "../thread/test_support.h"
#include "dross/notification/delivery.h"
#include "dross/notification/subscription.h"
#include "dross/thread/operation_queue.h"
#include "dross/thread/runloop.h"
#include "dross/thread/thread.h"
#include "notification/delivery_access.h"
#include "notification/subscription_access.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using dross_test::event;
using dross_test::kTimeout;

using receiver = dross::subscription_access::receiver;

// A receiver of int values, made on the calling thread, and the subscription
// for it. The receiver stands in for what sends notifications, which holds
// one of its own.
struct subscribed final {
    std::shared_ptr<receiver> sender_side;
    dross::subscription token;
};

subscribed subscribe(const dross::delivery& how, std::function<void(int)> function)
{
    auto which = std::make_shared<receiver>(dross::delivery_access::resolve(how), [function](const void* value) {
        function(*static_cast<const int*>(value));
    });
    auto token = dross::subscription_access::make(which);
    return { std::move(which), std::move(token) };
}

// A sending no one cancels.
std::shared_ptr<const receiver::sending> uncancelled()
{
    return std::make_shared<receiver::sending>();
}

void send(receiver& which, int value)
{
    which.deliver(std::make_shared<const int>(value), uncancelled(), false);
}

// Values a function was called with, from whichever thread called it.
class record final {
public:
    void add(int value)
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        _values.push_back(value);
        _threads.push_back(std::this_thread::get_id());
    }

    std::vector<int> values() const
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        return _values;
    }

    std::vector<std::thread::id> threads() const
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        return _threads;
    }

private:
    mutable std::mutex _mutex;
    std::vector<int> _values;
    std::vector<std::thread::id> _threads;
};

// Runs task on which, after every task queued there before it, and waits
// for it to return. The event is shared, so it outlives a set() still
// returning on the other thread.
bool run_on(dross::thread& which, std::function<void()> task)
{
    auto done = std::make_shared<event>();
    which.perform([task = std::move(task), done]() {
        task();
        done->set();
    });
    return done->wait();
}

// The id of the thread a dross::thread runs its tasks on.
std::thread::id id_of(dross::thread& which)
{
    std::thread::id id;
    EXPECT_TRUE(run_on(which, [&id]() {
        id = std::this_thread::get_id();
    }));
    return id;
}

// The run loop of a dross::thread.
dross::runloop loop_of(dross::thread& which)
{
    dross::runloop loop = dross::current_runloop();
    EXPECT_TRUE(run_on(which, [&loop]() {
        loop = dross::current_runloop();
    }));
    return loop;
}

}  // namespace

TEST(subscription_test, is_not_copyable)
{
    EXPECT_FALSE(std::is_copy_constructible_v<dross::subscription>);
    EXPECT_FALSE(std::is_copy_assignable_v<dross::subscription>);
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<dross::subscription>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<dross::subscription>);
}

TEST(subscription_test, a_default_constructed_one_is_empty_and_not_active)
{
    dross::subscription token;

    EXPECT_FALSE(token.active());
    token.reset();
    EXPECT_FALSE(token.active());
}

TEST(subscription_test, direct_runs_inside_the_sending_call_on_the_sending_thread)
{
    record calls;
    auto [sender_side, token] = subscribe(dross::delivery::direct(), [&calls](int value) {
        calls.add(value);
    });

    std::thread sender{ [&sender_side]() {
        send(*sender_side, 1);
    } };
    const auto sender_id = sender.get_id();
    sender.join();

    EXPECT_EQ(calls.values(), std::vector<int>{ 1 });
    EXPECT_EQ(calls.threads(), std::vector<std::thread::id>{ sender_id });
    EXPECT_TRUE(token.active());
}

TEST(subscription_test, queued_never_runs_inside_the_sending_call_even_on_its_own_thread)
{
    dross_test::reset_main_runloop();
    record calls;
    auto [sender_side, token] = subscribe(dross::delivery::queued(), [&calls](int value) {
        calls.add(value);
    });

    send(*sender_side, 1);
    EXPECT_TRUE(calls.values().empty());

    dross::current_runloop().run_pending();
    EXPECT_EQ(calls.values(), std::vector<int>{ 1 });
    EXPECT_EQ(calls.threads(), std::vector<std::thread::id>{ std::this_thread::get_id() });
}

TEST(subscription_test, queued_without_a_loop_names_the_subscribing_threads_loop)
{
    record calls;
    const auto how = dross::delivery::queued();
    auto subscriber = dross::thread::create().value();
    const auto subscriber_id = id_of(subscriber);

    std::shared_ptr<receiver> sender_side;
    dross::subscription token;
    ASSERT_TRUE(run_on(subscriber, [&]() {
        auto made = subscribe(how, [&calls](int value) {
            calls.add(value);
        });
        sender_side = std::move(made.sender_side);
        token = std::move(made.token);
    }));

    send(*sender_side, 1);
    ASSERT_TRUE(run_on(subscriber, []() {
    }));

    EXPECT_EQ(calls.values(), std::vector<int>{ 1 });
    EXPECT_EQ(calls.threads(), std::vector<std::thread::id>{ subscriber_id });
    token.reset();
    subscriber.quit();
    subscriber.join();
}

TEST(subscription_test, queued_with_a_loop_runs_on_that_loop)
{
    record calls;
    auto target = dross::thread::create().value();
    const auto target_id = id_of(target);

    auto [sender_side, token] = subscribe(dross::delivery::queued(loop_of(target)), [&calls](int value) {
        calls.add(value);
    });
    send(*sender_side, 1);
    ASSERT_TRUE(run_on(target, []() {
    }));

    EXPECT_EQ(calls.values(), std::vector<int>{ 1 });
    EXPECT_EQ(calls.threads(), std::vector<std::thread::id>{ target_id });
    token.reset();
    target.quit();
    target.join();
}

TEST(subscription_test, on_a_queue_runs_on_one_of_its_workers)
{
    record calls;
    auto called = std::make_shared<event>();
    auto workers = dross::operation_queue::create(1).value();
    auto [sender_side, token] = subscribe(dross::delivery::on(workers), [&calls, called](int value) {
        calls.add(value);
        called->set();
    });

    send(*sender_side, 1);
    ASSERT_TRUE(called->wait());

    EXPECT_EQ(calls.values(), std::vector<int>{ 1 });
    ASSERT_EQ(calls.threads().size(), 1U);
    EXPECT_NE(calls.threads().front(), std::this_thread::get_id());
    EXPECT_TRUE(token.active());
}

TEST(subscription_test, notifications_from_one_thread_start_in_the_order_sent)
{
    dross_test::reset_main_runloop();
    record calls;
    auto [sender_side, token] = subscribe(dross::delivery::queued(), [&calls](int value) {
        calls.add(value);
    });

    for (int value = 0; value < 5; ++value) {
        send(*sender_side, value);
    }
    dross::current_runloop().run_pending();

    EXPECT_EQ(calls.values(), (std::vector<int>{ 0, 1, 2, 3, 4 }));
}

TEST(subscription_test, notifications_on_a_queue_with_one_worker_run_one_at_a_time_in_the_order_sent)
{
    record calls;
    std::atomic<int> running{ 0 };
    std::atomic<bool> overlapped{ false };
    auto worker = dross::operation_queue::create(1).value();
    auto [sender_side, token] = subscribe(dross::delivery::on(worker), [&calls, &running, &overlapped](int value) {
        if (running.fetch_add(1) > 0) {
            overlapped = true;
        }
        calls.add(value);
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
        running.fetch_sub(1);
    });

    for (int value = 0; value < 5; ++value) {
        send(*sender_side, value);
    }
    ASSERT_TRUE(worker.wait_for(std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout)));

    EXPECT_EQ(calls.values(), (std::vector<int>{ 0, 1, 2, 3, 4 }));
    EXPECT_FALSE(overlapped.load());
}

TEST(subscription_test, every_subscriber_is_handed_the_same_value)
{
    dross_test::reset_main_runloop();
    std::vector<const void*> seen;
    auto record_address = [&seen](const void* value) {
        seen.push_back(value);
    };
    auto first = std::make_shared<receiver>(dross::delivery_access::resolve(dross::delivery::queued()), record_address);
    auto second = std::make_shared<receiver>(dross::delivery_access::resolve(dross::delivery::direct()), record_address);
    auto first_token = dross::subscription_access::make(first);
    auto second_token = dross::subscription_access::make(second);

    auto value = std::make_shared<const int>(7);
    const void* const address = value.get();
    const auto sent = uncancelled();
    first->deliver(value, sent, false);
    second->deliver(value, sent, false);
    value.reset();
    dross::current_runloop().run_pending();

    EXPECT_EQ(seen, (std::vector<const void*>{ address, address }));
}

TEST(subscription_test, reset_stops_a_queued_call_from_starting)
{
    dross_test::reset_main_runloop();
    record calls;
    auto [sender_side, token] = subscribe(dross::delivery::queued(), [&calls](int value) {
        calls.add(value);
    });

    send(*sender_side, 1);
    token.reset();
    dross::current_runloop().run_pending();

    EXPECT_TRUE(calls.values().empty());
    EXPECT_FALSE(token.active());
}

TEST(subscription_test, reset_stops_a_call_queued_on_a_queue_from_starting)
{
    record calls;
    auto workers = dross::operation_queue::create(1).value();
    auto release = std::make_shared<event>();
    auto blocking = std::make_shared<event>();
    workers.submit([release, blocking]() {
        blocking->set();
        release->wait();
    });
    ASSERT_TRUE(blocking->wait());

    auto [sender_side, token] = subscribe(dross::delivery::on(workers), [&calls](int value) {
        calls.add(value);
    });
    send(*sender_side, 1);
    token.reset();
    release->set();
    ASSERT_TRUE(workers.wait_for(std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout)));

    EXPECT_TRUE(calls.values().empty());
}

TEST(subscription_test, destroying_it_ends_it_as_reset_does)
{
    dross_test::reset_main_runloop();
    record calls;
    auto made = subscribe(dross::delivery::queued(), [&calls](int value) {
        calls.add(value);
    });
    auto sender_side = made.sender_side;

    send(*sender_side, 1);
    {
        auto token = std::move(made.token);
    }
    dross::current_runloop().run_pending();

    EXPECT_TRUE(calls.values().empty());
    EXPECT_FALSE(sender_side->active());
}

TEST(subscription_test, nothing_sent_after_reset_is_called)
{
    record calls;
    auto [sender_side, token] = subscribe(dross::delivery::direct(), [&calls](int value) {
        calls.add(value);
    });

    token.reset();
    send(*sender_side, 1);

    EXPECT_TRUE(calls.values().empty());
}

TEST(subscription_test, reset_from_inside_the_function_lets_that_call_finish)
{
    dross::subscription token;
    std::vector<int> steps;
    auto made = subscribe(dross::delivery::direct(), [&token, &steps](int value) {
        steps.push_back(value);
        token.reset();
        steps.push_back(value + 1);
    });
    token = std::move(made.token);

    send(*made.sender_side, 1);
    send(*made.sender_side, 10);

    EXPECT_EQ(steps, (std::vector<int>{ 1, 2 }));
    EXPECT_FALSE(token.active());
}

TEST(subscription_test, reset_does_not_wait_for_a_call_running_on_another_thread)
{
    auto running = std::make_shared<event>();
    auto release = std::make_shared<event>();
    std::atomic<bool> finished{ false };
    auto [sender_side, token] = subscribe(dross::delivery::direct(), [running, release, &finished](int) {
        running->set();
        release->wait();
        finished = true;
    });

    std::thread sender{ [&sender_side]() {
        send(*sender_side, 1);
    } };
    ASSERT_TRUE(running->wait());
    token.reset();
    EXPECT_FALSE(finished.load());

    release->set();
    sender.join();
    EXPECT_TRUE(finished.load());
}

TEST(subscription_test, moving_leaves_the_source_empty)
{
    auto [sender_side, token] = subscribe(dross::delivery::direct(), [](int) {
    });

    dross::subscription moved{ std::move(token) };

    EXPECT_TRUE(moved.active());
    EXPECT_FALSE(token.active());
}

TEST(subscription_test, move_assignment_ends_the_subscription_held_first)
{
    record calls;
    auto [first_side, token] = subscribe(dross::delivery::direct(), [&calls](int value) {
        calls.add(value);
    });
    auto second = subscribe(dross::delivery::direct(), [](int) {
    });

    token = std::move(second.token);
    send(*first_side, 1);

    EXPECT_TRUE(calls.values().empty());
    EXPECT_FALSE(first_side->active());
    EXPECT_TRUE(token.active());
}

TEST(subscription_test, one_on_a_thread_that_has_ended_is_not_active)
{
    record calls;
    auto target = dross::thread::create().value();
    auto [sender_side, token] = subscribe(dross::delivery::queued(loop_of(target)), [&calls](int value) {
        calls.add(value);
    });
    EXPECT_TRUE(token.active());

    target.quit();
    target.join();
    send(*sender_side, 1);

    EXPECT_FALSE(token.active());
    EXPECT_TRUE(calls.values().empty());
}

TEST(subscription_test, one_on_a_queue_that_has_shut_down_is_not_active)
{
    auto workers = dross::operation_queue::create(1).value();
    auto [sender_side, token] = subscribe(dross::delivery::on(workers), [](int) {
    });
    EXPECT_TRUE(token.active());

    ASSERT_TRUE(workers.shutdown(std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout)));

    EXPECT_FALSE(token.active());
}

TEST(subscription_test, notifications_queued_before_a_queue_stops_still_run)
{
    record calls;
    auto workers = dross::operation_queue::create(1).value();
    auto release = std::make_shared<event>();
    auto blocking = std::make_shared<event>();
    workers.submit([release, blocking]() {
        blocking->set();
        release->wait();
    });
    ASSERT_TRUE(blocking->wait());

    auto [sender_side, token] = subscribe(dross::delivery::on(workers), [&calls](int value) {
        calls.add(value);
    });
    send(*sender_side, 1);
    send(*sender_side, 2);
    EXPECT_FALSE(workers.shutdown(std::chrono::milliseconds::zero()));
    send(*sender_side, 3);
    release->set();
    ASSERT_TRUE(workers.shutdown(std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout)));

    EXPECT_EQ(calls.values(), (std::vector<int>{ 1, 2 }));
    EXPECT_FALSE(token.active());
}

TEST(subscription_test, it_does_not_keep_its_queue_going)
{
    std::optional<dross::operation_queue> workers{ dross::operation_queue::create(1).value() };
    auto [sender_side, token] = subscribe(dross::delivery::on(*workers), [](int) {
    });
    EXPECT_TRUE(token.active());

    workers.reset();

    EXPECT_FALSE(token.active());
}

TEST(subscription_test, a_queue_made_only_to_be_passed_leaves_it_inactive_from_the_start)
{
    auto [sender_side, token] = subscribe(dross::delivery::on(dross::operation_queue::create(1).value()), [](int) {
    });

    EXPECT_FALSE(token.active());
}

TEST(subscription_test, an_exception_from_a_direct_function_propagates_out_of_the_send)
{
    auto [sender_side, token] = subscribe(dross::delivery::direct(), [](int) {
        throw std::runtime_error{ "from the subscriber" };
    });

    EXPECT_THROW(send(*sender_side, 1), std::runtime_error);
}

TEST(subscription_test, an_exception_from_a_queued_function_propagates_out_of_the_running_call)
{
    dross_test::reset_main_runloop();
    auto [sender_side, token] = subscribe(dross::delivery::queued(), [](int) {
        throw std::runtime_error{ "from the subscriber" };
    });

    send(*sender_side, 1);

    EXPECT_THROW(dross::current_runloop().run_pending(), std::runtime_error);
}
