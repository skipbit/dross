#include "dross/thread/thread.h"
#include "dross/thread/timer.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {

// Generous enough that a real regression fails instead of flaking, but short
// enough that a genuine hang does not stall the suite.
constexpr auto kTimeout = std::chrono::seconds{ 5 };

// Every test here shares the main thread's loop, so the ones that use it
// start from a known state: no queued tasks, no quit request left behind.
// Every timer installed on it must be invalidated before the test returns,
// since there is no bulk equivalent of clear() for timers.
void reset_main_runloop()
{
    dross::runloop loop = dross::main_runloop();
    loop.clear();
    loop.run_for(std::chrono::milliseconds{ 1 });
}

// Runs loop in short slices until predicate() is true or bound passes.
// Short slices, rather than one run_for(bound), keep a test fast once its
// predicate is satisfied instead of waiting out the whole bound regardless.
template <typename Predicate>
void spin_until(dross::runloop& loop, Predicate&& predicate, std::chrono::milliseconds bound = kTimeout)
{
    const auto deadline = std::chrono::steady_clock::now() + bound;
    while (! predicate() && std::chrono::steady_clock::now() < deadline) {
        loop.run_for(std::chrono::milliseconds{ 10 });
    }
}

}  // namespace

TEST(timer_test, a_one_shot_fires_once_and_then_invalidates_itself)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    int fire_count = 0;
    dross::timer t = dross::timer::once(std::chrono::milliseconds{ 0 }, [&fire_count](dross::timer) {
        ++fire_count;
    }, loop);

    spin_until(loop, [&fire_count]() {
        return fire_count > 0;
    });

    EXPECT_EQ(fire_count, 1);
    EXPECT_FALSE(t.valid());
    EXPECT_EQ(loop.timer_count(), 0U);

    // Bounded proof it really does not fire again.
    loop.run_for(std::chrono::milliseconds{ 20 });
    EXPECT_EQ(fire_count, 1);
}

TEST(timer_test, a_repeating_timer_fires_more_than_once_and_stays_valid)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    std::atomic<int> fire_count{ 0 };
    dross::timer t = dross::timer::repeating(std::chrono::milliseconds{ 1 }, [&fire_count](dross::timer) {
        fire_count.fetch_add(1);
    }, loop);

    spin_until(loop, [&fire_count]() {
        return fire_count.load() >= 3;
    });

    EXPECT_GE(fire_count.load(), 3);
    EXPECT_TRUE(t.valid());

    t.invalidate();
    EXPECT_EQ(loop.timer_count(), 0U);
}

TEST(timer_test, the_callback_receives_the_timer_and_can_invalidate_itself_from_inside)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    int fire_count = 0;
    dross::timer t = dross::timer::repeating(std::chrono::milliseconds{ 0 }, [&fire_count](dross::timer self) {
        ++fire_count;
        if (fire_count >= 3) {
            self.invalidate();
        }
    }, loop);

    spin_until(loop, [&t]() {
        return ! t.valid();
    });

    EXPECT_EQ(fire_count, 3);
    EXPECT_FALSE(t.valid());
    EXPECT_EQ(loop.timer_count(), 0U);
}

TEST(timer_test, invalidate_before_the_first_fire_means_it_never_fires)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    int fire_count = 0;
    dross::timer t = dross::timer::once(std::chrono::milliseconds{ 50 }, [&fire_count](dross::timer) {
        ++fire_count;
    }, loop);

    t.invalidate();
    EXPECT_FALSE(t.valid());
    EXPECT_EQ(loop.timer_count(), 0U);

    loop.run_for(std::chrono::milliseconds{ 100 });
    EXPECT_EQ(fire_count, 0);
}

TEST(timer_test, invalidate_is_idempotent_and_safe_after_the_timer_has_ended)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    int fire_count = 0;
    dross::timer t = dross::timer::once(std::chrono::milliseconds{ 0 }, [&fire_count](dross::timer) {
        ++fire_count;
    }, loop);

    spin_until(loop, [&fire_count]() {
        return fire_count > 0;
    });
    ASSERT_FALSE(t.valid());

    // Safe and a no-op after the timer has already ended on its own.
    t.invalidate();
    t.invalidate();
    EXPECT_FALSE(t.valid());
    EXPECT_EQ(fire_count, 1);

    // Also idempotent before any fire at all.
    dross::timer t2 = dross::timer::once(std::chrono::milliseconds{ 50 }, [](dross::timer) {
    }, loop);
    t2.invalidate();
    t2.invalidate();
    EXPECT_FALSE(t2.valid());
}

TEST(timer_test, a_dropped_handle_does_not_stop_the_timer)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    int fire_count = 0;
    {
        dross::timer t = dross::timer::once(std::chrono::milliseconds{ 0 }, [&fire_count](dross::timer) {
            ++fire_count;
        }, loop);
        static_cast<void>(t);
    }
    // t is gone; the loop still holds the timer it named.
    EXPECT_EQ(loop.timer_count(), 1U);

    spin_until(loop, [&fire_count]() {
        return fire_count > 0;
    });
    EXPECT_EQ(fire_count, 1);
    EXPECT_EQ(loop.timer_count(), 0U);
}

TEST(timer_test, a_copied_handle_names_the_same_timer)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    dross::timer t = dross::timer::once(std::chrono::milliseconds{ 1000 }, [](dross::timer) {
    }, loop);
    dross::timer copy = t;

    EXPECT_TRUE(copy == t);

    t.invalidate();
    EXPECT_FALSE(copy.valid());

    copy.invalidate();
}

TEST(timer_test, installing_on_an_explicit_loop_runs_on_that_loops_thread)
{
    dross::thread worker;
    std::optional<dross::runloop> worker_loop;
    std::atomic<bool> captured{ false };

    ASSERT_TRUE(worker.perform([&worker_loop, &captured]() {
        worker_loop = dross::current_runloop();
        captured.store(true);
    }));

    const auto capture_deadline = std::chrono::steady_clock::now() + kTimeout;
    while (! captured.load() && std::chrono::steady_clock::now() < capture_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
    }
    ASSERT_TRUE(worker_loop.has_value());

    std::atomic<std::uint64_t> ran_on{ 0 };
    dross::timer t = dross::timer::once(std::chrono::milliseconds{ 0 }, [&ran_on](dross::timer) {
        ran_on.store(dross::current_thread().native_id().value());
    }, *worker_loop);
    static_cast<void>(t);

    const auto fire_deadline = std::chrono::steady_clock::now() + kTimeout;
    while (ran_on.load() == 0 && std::chrono::steady_clock::now() < fire_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
    }

    ASSERT_TRUE(worker.native_id().has_value());
    EXPECT_EQ(ran_on.load(), *worker.native_id());
    EXPECT_NE(ran_on.load(), dross::current_thread().native_id().value_or(0));

    ASSERT_TRUE(worker.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(worker.join_for(kTimeout));
}

TEST(timer_test, timer_count_rises_on_install_and_falls_on_invalidate_and_after_a_one_shot_fires)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    EXPECT_EQ(loop.timer_count(), 0U);

    dross::timer repeating = dross::timer::repeating(std::chrono::milliseconds{ 1000 }, [](dross::timer) {
    }, loop);
    EXPECT_EQ(loop.timer_count(), 1U);

    dross::timer one_shot = dross::timer::once(std::chrono::milliseconds{ 0 }, [](dross::timer) {
    }, loop);
    EXPECT_EQ(loop.timer_count(), 2U);

    spin_until(loop, [&one_shot]() {
        return ! one_shot.valid();
    });
    EXPECT_EQ(loop.timer_count(), 1U);

    repeating.invalidate();
    EXPECT_EQ(loop.timer_count(), 0U);
}

TEST(timer_test, run_pending_fires_a_timer_already_due_and_not_one_that_is_not)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    int due_fired = 0;
    int later_fired = 0;
    dross::timer due = dross::timer::once(std::chrono::milliseconds{ 0 }, [&due_fired](dross::timer) {
        ++due_fired;
    }, loop);
    dross::timer later = dross::timer::once(std::chrono::milliseconds{ 1000 }, [&later_fired](dross::timer) {
        ++later_fired;
    }, loop);

    // due's deadline has certainly passed by the time run_pending() looks.
    std::this_thread::sleep_for(std::chrono::milliseconds{ 5 });

    const auto started = std::chrono::steady_clock::now();
    EXPECT_EQ(loop.run_pending(), 1U);
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::milliseconds{ 100 });

    EXPECT_EQ(due_fired, 1);
    EXPECT_EQ(later_fired, 0);
    EXPECT_FALSE(due.valid());
    EXPECT_TRUE(later.valid());

    later.invalidate();
}

TEST(timer_test, a_late_fire_is_not_made_up)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    int fire_count = 0;
    dross::timer t = dross::timer::repeating(std::chrono::milliseconds{ 50 }, [&fire_count](dross::timer) {
        ++fire_count;
    }, loop);

    // The loop is not run at all while this elapses, so by the time it is,
    // the timer is eight intervals overdue. A catch-up implementation (one
    // that reschedules from the deadline that was due rather than from the
    // moment the fire was decided) works through all eight as soon as it
    // runs; this one fires once and looks an interval ahead.
    std::this_thread::sleep_for(std::chrono::milliseconds{ 400 });

    // A second fire needs the machine to stall for a whole interval between
    // the first one and the next look, which is what the interval is set
    // wide for: the gap to a catch-up implementation's eight stays clear
    // even when a loaded runner loses a slice.
    const std::size_t ran = loop.run_for(std::chrono::milliseconds{ 5 });
    EXPECT_LE(ran, 2U);
    EXPECT_LE(fire_count, 2);

    t.invalidate();
}

TEST(timer_test, a_slow_fire_does_not_shorten_another_timers_next_interval)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    // Both overdue by the time the loop is ever run, so the pass that opens
    // below claims both: slow first (its delay is shorter), then fast,
    // which is where a reschedule keyed to the pass boundary rather than to
    // the moment fast is claimed would land its next deadline in the past,
    // since slow's callback has by then run long past that boundary.
    dross::timer slow = dross::timer::once(std::chrono::milliseconds{ 0 }, [](dross::timer) {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 200 });
    }, loop);

    std::vector<std::chrono::steady_clock::time_point> fires;
    dross::timer fast = dross::timer::repeating(std::chrono::milliseconds{ 50 }, [&fires](dross::timer) {
        fires.push_back(std::chrono::steady_clock::now());
    }, loop);

    std::this_thread::sleep_for(std::chrono::milliseconds{ 60 });

    loop.run_for(std::chrono::milliseconds{ 350 });
    fast.invalidate();
    slow.invalidate();

    ASSERT_GE(fires.size(), 2U);
    for (std::size_t i = 1; i < fires.size(); ++i) {
        EXPECT_GE(fires[i] - fires[i - 1], std::chrono::milliseconds{ 40 }) << "gap before fire " << i << " was too short";
    }
}

TEST(timer_test, an_exception_from_the_callback_propagates_and_the_timer_stays_installed)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    int fire_count = 0;
    dross::timer t = dross::timer::repeating(std::chrono::milliseconds{ 0 }, [&fire_count](dross::timer) {
        ++fire_count;
        throw std::runtime_error{ "from a timer" };
    }, loop);

    EXPECT_THROW(loop.run_pending(), std::runtime_error);
    EXPECT_EQ(fire_count, 1);
    EXPECT_TRUE(t.valid());
    EXPECT_EQ(loop.timer_count(), 1U);

    // Still installed: it fires again on the next opportunity.
    EXPECT_THROW(loop.run_pending(), std::runtime_error);
    EXPECT_EQ(fire_count, 2);

    t.invalidate();
}

TEST(timer_test, an_exception_from_a_one_shot_callback_propagates_and_the_timer_is_gone)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    // Unlike a repeating timer, a one-shot is removed before it fires
    // regardless of whether the callback throws, so it does not stay
    // installed the way the repeating case above does.
    dross::timer t = dross::timer::once(std::chrono::milliseconds{ 0 }, [](dross::timer) {
        throw std::runtime_error{ "from a timer" };
    }, loop);

    EXPECT_THROW(loop.run_pending(), std::runtime_error);
    EXPECT_FALSE(t.valid());
    EXPECT_EQ(loop.timer_count(), 0U);
}

TEST(timer_test, an_empty_callback_is_not_installed)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    dross::timer t = dross::timer::once(std::chrono::milliseconds{ 0 }, std::function<void(dross::timer)>{}, loop);

    EXPECT_FALSE(t.valid());
    EXPECT_EQ(loop.timer_count(), 0U);
}

TEST(timer_test, installing_on_a_finished_loop_returns_an_invalid_handle)
{
    std::optional<dross::runloop> ended_loop;
    std::thread worker{ [&ended_loop]() {
        ended_loop = dross::current_runloop();
    } };
    worker.join();
    // The worker thread has ended; its loop is finished.

    ASSERT_TRUE(ended_loop.has_value());
    dross::timer t = dross::timer::once(std::chrono::milliseconds{ 0 }, [](dross::timer) {
    }, *ended_loop);

    EXPECT_FALSE(t.valid());
    EXPECT_EQ(ended_loop->timer_count(), 0U);
}

TEST(timer_test, a_huge_delay_does_not_overflow)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    // Exercises deadline::after() with the largest value a caller can give;
    // this is what the address-and-undefined sanitizer job installs to
    // catch a regression of the signed-overflow bug on this addition.
    dross::timer t = dross::timer::once(std::chrono::milliseconds::max(), [](dross::timer) {
    }, loop);

    EXPECT_TRUE(t.valid());
    EXPECT_EQ(loop.timer_count(), 1U);

    t.invalidate();
}

TEST(timer_test, installing_a_sooner_timer_cuts_short_a_wait_on_a_later_one)
{
    dross::thread worker;
    std::optional<dross::runloop> worker_loop;
    std::atomic<bool> captured{ false };

    ASSERT_TRUE(worker.perform([&worker_loop, &captured]() {
        worker_loop = dross::current_runloop();
        captured.store(true);
    }));
    const auto capture_deadline = std::chrono::steady_clock::now() + kTimeout;
    while (! captured.load() && std::chrono::steady_clock::now() < capture_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
    }
    ASSERT_TRUE(worker_loop.has_value());

    // Far enough out that the worker's already-running run() loop, with
    // nothing else queued, parks its wait on this deadline.
    dross::timer far = dross::timer::once(std::chrono::seconds{ 10 }, [](dross::timer) {
    }, *worker_loop);
    std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });

    // install_timer() notifies the condition variable the parked wait is
    // blocked on. A black-box test cannot isolate that one call by itself,
    // the way it isolates most behaviour here: a notify from anywhere on
    // this loop wakes the same wait regardless of which deadline it was
    // given, so the only way to observe a wake at all is by way of its
    // effect, which is what this checks: a sooner deadline installed while
    // parked on a later one is not missed until the later one's own stale
    // timeout.
    std::atomic<bool> soon_fired{ false };
    dross::timer soon = dross::timer::once(std::chrono::milliseconds{ 100 }, [&soon_fired](dross::timer) {
        soon_fired.store(true);
    }, *worker_loop);

    // Bounded well under far's 10-second deadline: if installing soon had
    // not woken the parked loop, this would only settle once that stale
    // deadline finally timed out on its own.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 2 };
    while (! soon_fired.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 5 });
    }

    EXPECT_TRUE(soon_fired.load());

    far.invalidate();
    ASSERT_TRUE(worker.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(worker.join_for(kTimeout));
}

TEST(timer_test, invalidate_from_another_thread_while_the_loop_is_running_it)
{
    dross::thread worker;

    std::atomic<int> fire_count{ 0 };
    std::optional<dross::timer> t;
    std::atomic<bool> installed{ false };

    ASSERT_TRUE(worker.perform([&t, &installed, &fire_count]() {
        t = dross::timer::repeating(std::chrono::milliseconds{ 1 }, [&fire_count](dross::timer) {
            fire_count.fetch_add(1);
        });
        installed.store(true);
    }));

    const auto install_deadline = std::chrono::steady_clock::now() + kTimeout;
    while (! installed.load() && std::chrono::steady_clock::now() < install_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
    }
    ASSERT_TRUE(installed.load());

    const auto fire_deadline = std::chrono::steady_clock::now() + kTimeout;
    while (fire_count.load() < 1 && std::chrono::steady_clock::now() < fire_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
    }
    ASSERT_GE(fire_count.load(), 1);

    // Racing this against the worker thread actively firing the same timer
    // is what exercises the thread sanitizer; see invalidate()'s own doc
    // comment on being callable from any thread.
    t->invalidate();
    EXPECT_FALSE(t->valid());

    // A fire the loop had already taken when invalidate() ran still finishes,
    // so the count is read after that one has landed. What invalidate()
    // promises is that no further fire is taken, which is what the second
    // reading checks.
    std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });
    const int settled = fire_count.load();
    std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });
    EXPECT_EQ(fire_count.load(), settled);

    ASSERT_TRUE(worker.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(worker.join_for(kTimeout));
}
