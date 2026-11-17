#include "../thread/test_support.h"
#include "dross/notification/delivery.h"
#include "dross/notification/observable.h"
#include "dross/notification/subscription.h"
#include "dross/thread/operation_queue.h"
#include "dross/thread/runloop.h"
#include "dross/thread/thread.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using dross_test::event;

struct no_equal final {
    int value;
};

template <typename T>
concept observable_of = requires { typename dross::observable<T>; };

static_assert(observable_of<int>);
static_assert(observable_of<std::string>);
static_assert(! observable_of<no_equal>);

static_assert(! std::is_copy_constructible_v<dross::observable<int>>);
static_assert(! std::is_copy_assignable_v<dross::observable<int>>);
static_assert(std::is_nothrow_move_constructible_v<dross::observable<int>>);
static_assert(std::is_copy_constructible_v<dross::observable<int>::proxy>);

// A function of neither one value nor two is refused.
template <typename F>
concept watches_int = requires(const dross::observable<int>& value, F function) { value.when_changed(function, dross::delivery::direct()); };

static_assert(watches_int<void (*)(const int&)>);
static_assert(watches_int<void (*)(const int&, const int&)>);
static_assert(! watches_int<void (*)()>);
static_assert(! watches_int<void (*)(const std::string&)>);

// Changes a function was given, from whichever thread called it.
class record final {
public:
    void add(int previous, int value)
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        _changes.emplace_back(previous, value);
        _threads.push_back(std::this_thread::get_id());
    }

    void add(int value)
    {
        add(0, value);
    }

    std::vector<std::pair<int, int>> changes() const
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        return _changes;
    }

    std::vector<int> values() const
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        std::vector<int> values;
        for (const auto& each : _changes) {
            values.push_back(each.second);
        }
        return values;
    }

    std::vector<std::thread::id> threads() const
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        return _threads;
    }

private:
    mutable std::mutex _mutex;
    std::vector<std::pair<int, int>> _changes;
    std::vector<std::thread::id> _threads;
};

using change = std::pair<int, int>;

// Runs task on which, after every task queued there before it, and waits
// for it to return.
bool run_on(dross::thread& which, std::function<void()> task)
{
    auto done = std::make_shared<event>();
    which.perform([task = std::move(task), done]() {
        task();
        done->set();
    });
    return done->wait();
}

// Runs task on a thread of its own, and waits for it to end.
void run_elsewhere(const std::function<void()>& task)
{
    std::thread other{ task };
    other.join();
}

}  // namespace

TEST(observable_test, get_returns_the_value_it_was_made_with)
{
    const dross::observable<int> volume{ 50 };

    EXPECT_EQ(volume.get(), 50);
}

TEST(observable_test, set_on_the_owners_thread_takes_effect_at_once)
{
    const dross::observable<int> volume{ 50 };

    EXPECT_TRUE(volume.set(60));

    EXPECT_EQ(volume.get(), 60);
}

TEST(observable_test, a_direct_function_is_called_on_each_change_with_the_value_after_it)
{
    const dross::observable<int> volume{ 50 };
    record calls;
    auto token = volume.when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::direct());

    volume.set(60);
    volume.set(70);

    EXPECT_EQ(calls.values(), (std::vector<int>{ 60, 70 }));
    EXPECT_EQ(calls.threads(), (std::vector<std::thread::id>{ std::this_thread::get_id(), std::this_thread::get_id() }));
}

TEST(observable_test, a_function_of_two_values_is_given_the_value_before_and_after)
{
    const dross::observable<int> volume{ 50 };
    record calls;
    auto token = volume.when_changed([&calls](const int& before, const int& now) {
        calls.add(before, now);
    }, dross::delivery::direct());

    volume.set(60);
    volume.set(70);

    EXPECT_EQ(calls.changes(), (std::vector<change>{ { 50, 60 }, { 60, 70 } }));
}

TEST(observable_test, it_is_not_called_for_the_value_there_already_was)
{
    const dross::observable<int> volume{ 50 };
    record calls;

    auto direct = volume.when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::direct());
    auto queued = volume.when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::queued());
    dross::current_runloop().run_pending();

    EXPECT_TRUE(calls.values().empty());
}

TEST(observable_test, setting_an_equal_value_changes_nothing_and_calls_nothing)
{
    const dross::observable<std::string> name{ "first" };
    std::vector<std::string> seen;
    auto token = name.when_changed([&seen](const std::string& now) {
        seen.push_back(now);
    }, dross::delivery::direct());

    EXPECT_TRUE(name.set("first"));
    name.set("second");
    name.set("second");

    EXPECT_EQ(seen, (std::vector<std::string>{ "second" }));
}

TEST(observable_test, a_queued_function_runs_on_the_loop_and_never_inside_set)
{
    dross_test::reset_main_runloop();
    const dross::observable<int> volume{ 50 };
    record calls;
    auto token = volume.when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::queued());

    volume.set(60);
    EXPECT_TRUE(calls.values().empty());
    dross::current_runloop().run_pending();

    EXPECT_EQ(calls.values(), (std::vector<int>{ 60 }));
}

TEST(observable_test, a_queued_function_is_given_the_latest_change_and_skips_those_in_between)
{
    dross_test::reset_main_runloop();
    const dross::observable<int> volume{ 1 };
    record calls;
    auto token = volume.when_changed([&calls](const int& before, const int& now) {
        calls.add(before, now);
    }, dross::delivery::queued());

    volume.set(2);
    volume.set(3);
    volume.set(4);
    dross::current_runloop().run_pending();

    EXPECT_EQ(calls.changes(), (std::vector<change>{ { 3, 4 } }));
}

TEST(observable_test, a_change_after_a_queued_call_has_run_is_queued_again)
{
    dross_test::reset_main_runloop();
    const dross::observable<int> volume{ 1 };
    record calls;
    auto token = volume.when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::queued());

    volume.set(2);
    dross::current_runloop().run_pending();
    volume.set(3);
    dross::current_runloop().run_pending();

    EXPECT_EQ(calls.values(), (std::vector<int>{ 2, 3 }));
}

TEST(observable_test, a_function_on_a_queue_runs_on_one_of_its_workers)
{
    const dross::observable<int> volume{ 1 };
    record calls;
    auto called = std::make_shared<event>();
    dross::operation_queue workers{ 1 };
    auto token = volume.when_changed([&calls, called](const int& now) {
        calls.add(now);
        called->set();
    }, dross::delivery::on(workers));

    volume.set(2);
    ASSERT_TRUE(called->wait());

    EXPECT_EQ(calls.values(), (std::vector<int>{ 2 }));
    ASSERT_EQ(calls.threads().size(), 1U);
    EXPECT_NE(calls.threads().front(), std::this_thread::get_id());
}

TEST(observable_test, a_change_made_inside_a_direct_function_reaches_every_function_after_the_one_being_handed_on)
{
    dross_test::reset_main_runloop();
    const dross::observable<int> volume{ 1 };
    std::vector<std::string> seen;
    std::vector<int> read_back;
    auto first = volume.when_changed([&](const int& before, const int& now) {
        seen.push_back("first " + std::to_string(before) + "->" + std::to_string(now));
        if (now == 2) {
            volume.set(3);
            read_back.push_back(volume.get());
        }
    }, dross::delivery::direct());
    auto second = volume.when_changed([&](const int& before, const int& now) {
        seen.push_back("second " + std::to_string(before) + "->" + std::to_string(now));
    }, dross::delivery::direct());
    record queued;
    auto third = volume.when_changed([&queued](const int& before, const int& now) {
        queued.add(before, now);
    }, dross::delivery::queued());

    volume.set(2);
    dross::current_runloop().run_pending();

    EXPECT_EQ(seen, (std::vector<std::string>{ "first 1->2", "second 1->2", "first 2->3", "second 2->3" }));
    EXPECT_EQ(read_back, (std::vector<int>{ 3 }));
    EXPECT_EQ(queued.changes(), (std::vector<change>{ { 2, 3 } }));
    EXPECT_EQ(volume.get(), 3);
}

TEST(observable_test, an_empty_function_gives_a_subscription_never_called_and_not_active)
{
    const dross::observable<int> volume{ 1 };
    void (*null_function)(const int&) = nullptr;
    const std::function<void(const int&, const int&)> empty;

    auto from_pointer = volume.when_changed(null_function, dross::delivery::direct());
    auto from_function = volume.when_changed(empty, dross::delivery::direct());
    volume.set(2);

    EXPECT_FALSE(from_pointer.active());
    EXPECT_FALSE(from_function.active());
    EXPECT_EQ(volume.get(), 2);
}

TEST(observable_test, reset_stops_the_calls)
{
    dross_test::reset_main_runloop();
    const dross::observable<int> volume{ 1 };
    record calls;
    auto direct = volume.when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::direct());
    auto queued = volume.when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::queued());

    volume.set(2);
    EXPECT_TRUE(direct.active());
    direct.reset();
    queued.reset();
    volume.set(3);
    dross::current_runloop().run_pending();

    EXPECT_EQ(calls.values(), (std::vector<int>{ 2 }));
}

TEST(observable_test, an_ended_functions_captures_are_let_go_when_it_ends)
{
    const dross::observable<int> volume{ 1 };
    auto held = std::make_shared<int>(0);
    const std::weak_ptr<int> watch = held;
    auto token = volume.when_changed([held = std::move(held)](const int&) {
    }, dross::delivery::direct());

    token.reset();

    EXPECT_TRUE(watch.expired());
}

TEST(observable_test, subscribing_and_resetting_inside_a_function_is_allowed)
{
    const dross::observable<int> volume{ 1 };
    record calls;
    dross::subscription inner;
    dross::subscription outer;
    outer = volume.when_changed([&](const int& now) {
        if (now == 2) {
            inner = volume.when_changed([&calls](const int& later) {
                calls.add(later);
            }, dross::delivery::direct());
            outer.reset();
        }
    }, dross::delivery::direct());

    volume.set(2);
    volume.set(3);

    EXPECT_FALSE(outer.active());
    EXPECT_EQ(calls.values(), (std::vector<int>{ 3 }));
}

TEST(observable_test, subscribing_and_resetting_inside_a_function_does_not_stop_the_change_reaching_the_functions_after_it)
{
    const dross::observable<int> volume{ 1 };
    record calls;
    dross::subscription added;
    dross::subscription bystander;
    bool first_call = true;
    auto first = volume.when_changed([&](const int&) {
        if (first_call) {
            first_call = false;
            added = volume.when_changed([](const int&) {
            }, dross::delivery::direct());
            bystander.reset();
        }
    }, dross::delivery::direct());
    auto second = volume.when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::direct());
    bystander = volume.when_changed([](const int&) {
    }, dross::delivery::direct());

    volume.set(2);

    EXPECT_EQ(calls.values(), (std::vector<int>{ 2 }));
}

TEST(observable_test, a_function_subscribed_inside_another_is_not_given_a_change_made_before_it_subscribed)
{
    const dross::observable<int> volume{ 1 };
    record calls;
    dross::subscription later;
    auto first = volume.when_changed([&](const int& now) {
        if (now == 2) {
            volume.set(3);
            later = volume.when_changed([&calls](const int& before, const int& after) {
                calls.add(before, after);
            }, dross::delivery::direct());
        }
    }, dross::delivery::direct());

    volume.set(2);
    EXPECT_TRUE(calls.changes().empty());
    EXPECT_EQ(volume.get(), 3);
    volume.set(4);

    EXPECT_EQ(calls.changes(), (std::vector<change>{ { 3, 4 } }));
}

TEST(observable_test, a_set_from_another_thread_takes_effect_when_the_owners_loop_runs_it)
{
    dross_test::reset_main_runloop();
    const dross::observable<int> volume{ 1 };
    record calls;
    auto token = volume.when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::direct());

    bool accepted = false;
    run_elsewhere([&volume, &accepted]() {
        accepted = volume.set(2);
    });
    EXPECT_TRUE(accepted);
    EXPECT_EQ(volume.get(), 1);
    dross::current_runloop().run_pending();

    EXPECT_EQ(volume.get(), 2);
    EXPECT_EQ(calls.values(), (std::vector<int>{ 2 }));
    EXPECT_EQ(calls.threads(), (std::vector<std::thread::id>{ std::this_thread::get_id() }));
}

TEST(observable_test, sets_from_one_thread_take_effect_in_the_order_they_were_posted)
{
    dross_test::reset_main_runloop();
    const dross::observable<int> volume{ 0 };
    record calls;
    auto token = volume.when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::direct());

    run_elsewhere([proxy = volume.make_proxy()]() {
        for (int each = 1; each <= 5; ++each) {
            proxy.set(each);
        }
    });
    dross::current_runloop().run_pending();

    EXPECT_EQ(calls.values(), (std::vector<int>{ 1, 2, 3, 4, 5 }));
}

TEST(observable_test, a_function_subscribed_from_another_thread_runs_where_its_delivery_says)
{
    dross_test::reset_main_runloop();
    const dross::observable<int> volume{ 1 };
    record calls;
    dross::thread watcher;
    std::thread::id watcher_id;
    dross::subscription direct;
    dross::subscription queued;
    ASSERT_TRUE(run_on(watcher, [&]() {
        watcher_id = std::this_thread::get_id();
        direct = volume.when_changed([&calls](const int& now) {
            calls.add(now);
        }, dross::delivery::direct());
        queued = volume.when_changed([&calls](const int& now) {
            calls.add(now * 10);
        }, dross::delivery::queued());
    }));

    volume.set(2);
    ASSERT_TRUE(run_on(watcher, []() {
    }));

    EXPECT_EQ(calls.values(), (std::vector<int>{ 2, 20 }));
    EXPECT_EQ(calls.threads(), (std::vector<std::thread::id>{ std::this_thread::get_id(), watcher_id }));
    direct.reset();
    queued.reset();
    watcher.quit();
    watcher.join();
}

TEST(observable_test, a_proxy_sets_the_value_from_another_thread)
{
    dross_test::reset_main_runloop();
    const dross::observable<int> volume{ 1 };
    auto proxy = volume.make_proxy();

    bool accepted = false;
    run_elsewhere([proxy, &accepted]() {
        accepted = proxy.set(2);
    });
    dross::current_runloop().run_pending();

    EXPECT_TRUE(accepted);
    EXPECT_EQ(volume.get(), 2);
}

TEST(observable_test, a_proxy_on_the_owners_thread_sets_at_once)
{
    const dross::observable<int> volume{ 1 };

    EXPECT_TRUE(volume.make_proxy().set(2));

    EXPECT_EQ(volume.get(), 2);
}

TEST(observable_test, once_the_observable_has_gone_a_proxy_sets_nothing_and_its_subscriptions_are_not_active)
{
    std::optional<dross::observable<int>> volume{ std::in_place, 1 };
    auto proxy = volume->make_proxy();
    auto token = volume->when_changed([](const int&) {
    }, dross::delivery::direct());
    EXPECT_TRUE(token.active());

    volume.reset();

    EXPECT_FALSE(proxy.set(2));
    EXPECT_FALSE(token.active());
}

TEST(observable_test, a_value_posted_to_an_observable_that_then_goes_is_dropped)
{
    dross_test::reset_main_runloop();
    std::optional<dross::observable<int>> volume{ std::in_place, 1 };
    auto proxy = volume->make_proxy();
    record calls;
    auto token = volume->when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::direct());

    run_elsewhere([proxy]() {
        EXPECT_TRUE(proxy.set(2));
    });
    volume.reset();
    dross::current_runloop().run_pending();

    EXPECT_TRUE(calls.values().empty());
}

TEST(observable_test, once_the_owners_thread_has_ended_a_set_from_elsewhere_returns_false)
{
    std::optional<dross::observable<int>> volume;
    {
        dross::thread owner;
        ASSERT_TRUE(run_on(owner, [&volume]() {
            volume.emplace(1);
        }));
        owner.quit();
        owner.join();
    }

    EXPECT_FALSE(volume->set(2));
    EXPECT_FALSE(volume->make_proxy().set(2));
}

TEST(observable_test, moving_keeps_the_owner_and_the_subscriptions)
{
    dross::observable<int> first{ 1 };
    record calls;
    auto token = first.when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::direct());
    auto proxy = first.make_proxy();

    dross::observable<int> second{ std::move(first) };
    second.set(2);
    proxy.set(3);

    EXPECT_EQ(second.get(), 3);
    EXPECT_EQ(calls.values(), (std::vector<int>{ 2, 3 }));
}

TEST(observable_test, a_moved_from_observable_sets_nothing_and_gives_subscriptions_that_are_not_active)
{
    dross::observable<int> first{ 1 };
    dross::observable<int> second{ std::move(first) };
    record calls;

    auto token = first.when_changed([&calls](const int& now) {
        calls.add(now);
    }, dross::delivery::direct());

    EXPECT_FALSE(token.active());
    EXPECT_FALSE(first.set(2));
    EXPECT_FALSE(first.make_proxy().set(3));
    EXPECT_EQ(second.get(), 1);
    EXPECT_TRUE(calls.values().empty());
}

TEST(observable_test, a_moved_from_observable_can_be_assigned_to_again)
{
    dross::observable<int> first{ 1 };
    dross::observable<int> second{ std::move(first) };

    first = dross::observable<int>{ 5 };

    EXPECT_EQ(first.get(), 5);
    EXPECT_TRUE(first.set(6));
    EXPECT_EQ(first.get(), 6);
}

TEST(observable_test, an_exception_from_a_direct_function_propagates_out_of_set_and_later_changes_still_arrive)
{
    const dross::observable<int> volume{ 1 };
    record calls;
    auto token = volume.when_changed([&calls](const int& now) {
        if (now == 2) {
            throw std::runtime_error{ "from the function" };
        }
        calls.add(now);
    }, dross::delivery::direct());

    EXPECT_THROW(volume.set(2), std::runtime_error);
    EXPECT_EQ(volume.get(), 2);
    volume.set(3);

    EXPECT_EQ(calls.values(), (std::vector<int>{ 3 }));
}

TEST(observable_death_test, get_from_a_thread_that_does_not_own_it_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    const dross::observable<int> volume{ 1 };

    EXPECT_DEATH(run_elsewhere([&volume]() {
        (void)volume.get();
    }),
                 "dross: observable::get: called from a thread that does not own the observable");
}

TEST(observable_death_test, get_once_the_owners_thread_has_ended_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    std::optional<dross::observable<int>> volume;
    {
        dross::thread owner;
        ASSERT_TRUE(run_on(owner, [&volume]() {
            volume.emplace(1);
        }));
        owner.quit();
        owner.join();
    }

    EXPECT_DEATH((void)volume->get(), "does not own the observable");
}

TEST(observable_death_test, get_on_a_moved_from_observable_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    dross::observable<int> first{ 1 };
    const dross::observable<int> second{ std::move(first) };

    EXPECT_DEATH((void)first.get(), "dross: observable::get: called on an observable that has been moved from");
}
