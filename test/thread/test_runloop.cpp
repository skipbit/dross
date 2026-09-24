#include "dross/thread/runloop.h"
#include "dross/thread/timer.h"
#include "manual_time_source.h"
#include "observed_time_source.h"
#include "test_support.h"
#include "thread/runloop_access.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

using dross_test::kTimeout;
using dross_test::reset_main_runloop;

}  // namespace

TEST(runloop_test, two_handles_from_one_thread_name_the_same_loop)
{
    EXPECT_TRUE(dross::current_runloop() == dross::current_runloop());
}

TEST(runloop_test, the_main_threads_current_loop_is_the_main_loop)
{
    EXPECT_TRUE(dross::current_runloop() == dross::main_runloop());
}

TEST(runloop_test, a_copied_handle_names_the_same_loop)
{
    const dross::runloop original = dross::current_runloop();
    const dross::runloop copy = original;

    EXPECT_TRUE(copy == original);
}

TEST(runloop_test, another_thread_has_its_own_loop)
{
    const dross::runloop mine = dross::current_runloop();

    std::optional<dross::runloop> theirs;
    std::thread worker{ [&theirs]() {
        theirs = dross::current_runloop();
    } };
    worker.join();

    ASSERT_TRUE(theirs.has_value());
    EXPECT_FALSE(*theirs == mine);
}

TEST(runloop_test, a_worker_reaches_the_same_main_loop)
{
    const dross::runloop here = dross::main_runloop();

    std::optional<dross::runloop> there;
    std::thread worker{ [&there]() {
        there = dross::main_runloop();
    } };
    worker.join();

    ASSERT_TRUE(there.has_value());
    EXPECT_TRUE(*there == here);
}

TEST(runloop_test, perform_queues_without_running)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    EXPECT_TRUE(loop.empty());
    EXPECT_TRUE(loop.perform([]() {
    }));
    EXPECT_EQ(loop.pending_count(), 1U);
    EXPECT_FALSE(loop.empty());

    loop.clear();
    EXPECT_TRUE(loop.empty());
}

TEST(runloop_test, perform_rejects_an_empty_task)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    EXPECT_FALSE(loop.perform(std::function<void()>{}));
    EXPECT_TRUE(loop.empty());
}

TEST(runloop_test, perform_reports_false_after_the_thread_ended)
{
    std::optional<dross::runloop> theirs;
    std::thread worker{ [&theirs]() {
        theirs = dross::current_runloop();
    } };
    worker.join();

    ASSERT_TRUE(theirs.has_value());
    EXPECT_FALSE(theirs->perform([]() {
    }));
}

TEST(runloop_test, run_pending_runs_what_is_queued_in_order)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    std::vector<int> order;
    loop.perform([&order]() {
        order.push_back(1);
    });
    loop.perform([&order]() {
        order.push_back(2);
    });

    EXPECT_EQ(loop.run_pending(), 2U);
    EXPECT_EQ(order, (std::vector<int>{ 1, 2 }));
    EXPECT_TRUE(loop.empty());
}

TEST(runloop_test, run_pending_leaves_a_task_that_a_task_added)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([loop]() mutable {
        loop.perform([]() {
        });
    });

    EXPECT_EQ(loop.run_pending(), 1U);
    EXPECT_EQ(loop.pending_count(), 1U);

    loop.clear();
}

TEST(runloop_test, run_pending_does_not_run_a_task_added_by_a_task_that_cleared)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    bool c_ran = false;
    loop.perform([loop, &c_ran]() mutable {
        loop.clear();
        loop.perform([&c_ran]() {
            c_ran = true;
        });
    });
    loop.perform([]() {
    });

    EXPECT_EQ(loop.run_pending(), 1U);
    EXPECT_FALSE(c_ran);
    EXPECT_EQ(loop.pending_count(), 1U);

    loop.clear();
}

TEST(runloop_test, clear_discards_the_queue)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    bool ran = false;
    loop.perform([&ran]() {
        ran = true;
    });
    loop.clear();

    EXPECT_EQ(loop.run_pending(), 0U);
    EXPECT_FALSE(ran);
}

TEST(runloop_test, quit_ends_run_and_keeps_the_queue)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([]() {
    });
    loop.quit();

    EXPECT_EQ(loop.run(), 0U);
    EXPECT_EQ(loop.pending_count(), 1U);

    loop.clear();
}

TEST(runloop_test, run_pending_does_not_consume_a_pending_quit)
{
    dross_test::manual_loop manual;
    dross::runloop& loop = manual.loop;

    loop.quit();

    EXPECT_EQ(loop.run_pending(), 0U);

    // A surviving quit makes run_for return at once; a consumed one makes it
    // wait out the full timeout, which would move this clock.
    const auto started = manual.clock->now();
    EXPECT_EQ(loop.run_for(std::chrono::milliseconds{ 200 }), 0U);
    EXPECT_EQ(manual.clock->now(), started);
}

TEST(runloop_test, a_quit_from_another_thread_ends_a_waiting_run)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    dross::runloop loop = dross::runloop_access::standalone(source);

    // Quits only once run() below is waiting, so the quit is what wakes it.
    std::atomic<bool> saw_wait{ false };
    std::thread worker{ [loop, source, &saw_wait]() mutable {
        saw_wait.store(source->await_untimed_waits(1));
        loop.quit();
    } };

    EXPECT_EQ(loop.run(), 0U);
    worker.join();
    EXPECT_TRUE(saw_wait.load());
}

TEST(runloop_test, run_returns_the_number_of_tasks_it_ran)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([]() {
    });
    loop.perform([]() {
    });
    loop.perform([loop]() mutable {
        loop.quit();
    });

    EXPECT_EQ(loop.run(), 3U);
}

TEST(runloop_test, run_one_returns_false_when_quit_is_pending)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([]() {
    });
    loop.quit();

    EXPECT_FALSE(loop.run_one());

    loop.clear();
}

TEST(runloop_test, a_nested_run_one_does_not_consume_the_outer_quit)
{
    dross_test::manual_loop manual;
    dross::runloop& loop = manual.loop;

    loop.perform([loop]() mutable {
        loop.quit();
        loop.run_one();
    });

    // The quit left for the outer run ends it as soon as the task returns. Had
    // the nested run_one() consumed it, the outer run would wait out the full
    // timeout, which would move this clock.
    const auto started = manual.clock->now();
    EXPECT_EQ(loop.run_for(kTimeout), 1U);
    EXPECT_EQ(manual.clock->now(), started);
}

TEST(runloop_test, a_worker_hands_work_back_to_the_main_thread)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    std::atomic<std::thread::id> ran_on{};
    std::atomic<bool> queued{ false };
    std::thread worker{ [loop, &ran_on, &queued]() mutable {
        queued.store(loop.perform([loop, &ran_on]() mutable {
            ran_on.store(std::this_thread::get_id());
            loop.quit();
        }));
    } };

    EXPECT_EQ(loop.run_for(std::chrono::seconds{ 5 }), 1U);
    EXPECT_EQ(ran_on.load(), std::this_thread::get_id());

    worker.join();
    EXPECT_TRUE(queued.load());
}

TEST(runloop_test, tasks_from_many_threads_all_arrive)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    dross::runloop loop = dross::runloop_access::standalone(source);

    constexpr int kPosters = 4;
    constexpr int kPerPoster = 50;
    constexpr int kTotal = kPosters * kPerPoster;

    std::atomic<int> ran{ 0 };
    std::atomic<int> queued{ 0 };
    std::atomic<int> saw_wait{ 0 };
    std::vector<std::thread> posters;
    posters.reserve(kPosters);
    for (int poster = 0; poster < kPosters; ++poster) {
        posters.emplace_back([loop, source, &ran, &queued, &saw_wait]() mutable {
            // Posts only once the loop is waiting, so the first task to
            // arrive wakes it from another thread.
            if (source->await_waits(1)) {
                saw_wait.fetch_add(1);
            }
            for (int n = 0; n < kPerPoster; ++n) {
                if (loop.perform([loop, &ran]() mutable {
                    // The last task to run ends the run below, however the
                    // posters' tasks interleave.
                    if (ran.fetch_add(1) + 1 == kTotal) {
                        loop.quit();
                    }
                })) {
                    queued.fetch_add(1);
                }
            }
        });
    }

    const std::size_t seen = loop.run_for(kTimeout);

    for (auto& poster : posters) {
        poster.join();
    }

    EXPECT_EQ(saw_wait.load(), kPosters);
    EXPECT_EQ(seen, static_cast<std::size_t>(kTotal));
    EXPECT_EQ(queued.load(), kTotal);
    EXPECT_EQ(ran.load(), kTotal);
    EXPECT_TRUE(loop.empty());
}

TEST(runloop_test, run_for_returns_when_the_timeout_passes)
{
    dross_test::manual_loop manual;
    dross::runloop& loop = manual.loop;

    const auto started = manual.clock->now();
    EXPECT_EQ(loop.run_for(std::chrono::milliseconds{ 20 }), 0U);
    EXPECT_EQ(manual.clock->now() - started, std::chrono::milliseconds{ 20 });
}

TEST(runloop_test, run_for_runs_what_is_queued)
{
    dross_test::manual_loop manual;
    dross::runloop& loop = manual.loop;

    loop.perform([]() {
    });
    loop.perform([]() {
    });

    EXPECT_EQ(loop.run_for(std::chrono::milliseconds{ 20 }), 2U);
}

TEST(runloop_test, run_for_zero_runs_nothing)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([]() {
    });

    EXPECT_EQ(loop.run_for(std::chrono::milliseconds{ 0 }), 0U);
    EXPECT_EQ(loop.pending_count(), 1U);

    loop.clear();
}

TEST(runloop_test, run_for_with_a_huge_timeout_does_not_overflow)
{
    dross_test::manual_loop manual;
    dross::runloop& loop = manual.loop;

    loop.perform([loop]() mutable {
        loop.quit();
    });

    // An overflowed deadline is already past, so run_for would run nothing.
    const auto started = manual.clock->now();
    EXPECT_EQ(loop.run_for(std::chrono::milliseconds::max()), 1U);
    EXPECT_EQ(manual.clock->now(), started);
}

TEST(runloop_test, is_running_is_true_inside_a_task)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    bool seen = false;
    loop.perform([loop, &seen]() mutable {
        seen = loop.is_running();
    });

    EXPECT_FALSE(loop.is_running());
    loop.run_pending();

    EXPECT_TRUE(seen);
    EXPECT_FALSE(loop.is_running());
}

TEST(runloop_test, run_pending_from_inside_a_task_restores_the_outer_running_state)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    bool seen_inside_inner_task = false;
    bool seen_after_inner_run = false;
    loop.perform([loop, &seen_inside_inner_task, &seen_after_inner_run]() mutable {
        loop.perform([loop, &seen_inside_inner_task]() mutable {
            seen_inside_inner_task = loop.is_running();
        });
        loop.run_pending();
        seen_after_inner_run = loop.is_running();
    });

    loop.run_pending();

    EXPECT_TRUE(seen_inside_inner_task);
    EXPECT_TRUE(seen_after_inner_run);
    EXPECT_FALSE(loop.is_running());
}

TEST(runloop_test, an_exception_from_a_task_propagates)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([]() {
        throw std::runtime_error{ "from a task" };
    });
    loop.perform([]() {
    });

    EXPECT_THROW(loop.run_pending(), std::runtime_error);
    EXPECT_EQ(loop.pending_count(), 1U);
    EXPECT_FALSE(loop.is_running());

    loop.clear();
}

TEST(runloop_test, an_exception_from_a_task_propagates_out_of_run)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([]() {
        throw std::runtime_error{ "from a task" };
    });

    EXPECT_THROW(loop.run(), std::runtime_error);
    EXPECT_FALSE(loop.is_running());

    bool ran = false;
    loop.perform([&ran]() {
        ran = true;
    });
    EXPECT_EQ(loop.run_pending(), 1U);
    EXPECT_TRUE(ran);
}

TEST(runloop_test, a_due_timer_fires_before_a_queued_task)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    std::vector<int> order;
    loop.perform([&order]() {
        order.push_back(2);
    });
    dross::timer t = dross::timer::once(std::chrono::milliseconds{ 0 }, [&order](dross::timer) {
        order.push_back(1);
    }, loop);

    EXPECT_EQ(loop.run_pending(), 2U);
    EXPECT_EQ(order, (std::vector<int>{ 1, 2 }));
    EXPECT_FALSE(t.valid());
}

TEST(runloop_test, run_pending_does_not_run_a_task_a_timer_posts_in_the_same_call)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    int timer_fired = 0;
    int task_ran = 0;
    dross::timer t = dross::timer::once(std::chrono::milliseconds{ 0 }, [&loop, &timer_fired, &task_ran](dross::timer) {
        ++timer_fired;
        loop.perform([&task_ran]() {
            ++task_ran;
        });
    }, loop);

    // The cutoff for this call's own task drain must be read before the
    // timer above runs, not after: otherwise the task it posts falls inside
    // this call's own cutoff and runs in the same call that queued it.
    EXPECT_EQ(loop.run_pending(), 1U);
    EXPECT_EQ(timer_fired, 1);
    EXPECT_EQ(task_ran, 0);
    EXPECT_EQ(loop.pending_count(), 1U);

    loop.clear();
    EXPECT_FALSE(t.valid());
}

TEST(runloop_test, run_for_with_the_most_negative_timeout_does_not_overflow)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    // Exercises deadline::after() with the most negative value a caller can
    // give; this is what the address-and-undefined sanitizer job installs
    // to catch a regression of the signed-overflow bug on this conversion.
    EXPECT_EQ(loop.run_for(std::chrono::milliseconds::min()), 0U);
}

TEST(runloop_test, ending_a_loop_releases_its_queued_tasks_and_timers_on_that_thread)
{
    // A shared_ptr with a custom deleter, rather than a counting member with
    // its own destructor, so incidental copies made while the task or timer
    // travels through std::function and this loop's own containers do not
    // themselves count as a release: only the very last reference does.
    auto released = std::make_shared<std::atomic<int>>(0);
    auto released_on_owner = std::make_shared<std::atomic<int>>(0);

    std::thread worker{ [released, released_on_owner]() {
        const auto owner = std::this_thread::get_id();
        dross::runloop loop = dross::current_runloop();

        const auto make_resource = [released, released_on_owner, owner]() {
            return std::shared_ptr<int>(new int{ 0 }, [released, released_on_owner, owner](int* p) {
                delete p;
                released->fetch_add(1);
                if (std::this_thread::get_id() == owner) {
                    released_on_owner->fetch_add(1);
                }
            });
        };

        loop.perform([resource = make_resource()]() {
        });
        static_cast<void>(dross::timer::once(std::chrono::hours{ 1 }, [resource = make_resource()](dross::timer) {
        }, loop));

        // The thread ends here without ever calling run(): neither the task
        // nor the timer above ever fires.
    } };
    worker.join();

    EXPECT_EQ(released->load(), 2);
    EXPECT_EQ(released_on_owner->load(), 2);
}

TEST(runloop_test, current_runloop_is_defined_from_a_thread_local_destructor_that_outlives_the_holder)
{
    struct destructor_probe final {
        std::function<void()> on_destroy;
        ~destructor_probe()
        {
            on_destroy();
        }
    };

    std::optional<bool> performed;

    std::thread worker{ [&performed]() {
        // Constructed before this thread ever touches its own run loop, so
        // it is destroyed after that loop's own holder, the same ordering
        // that makes a user's own thread-local destructor run after this
        // library's when the user's was constructed first. See
        // current_thread_is_defined_from_a_thread_local_destructor_that_outlives_the_holder
        // in test_thread.cpp for the equivalent covering current_thread().
        thread_local destructor_probe probe{ [&performed]() {
            dross::runloop loop = dross::current_runloop();
            performed = loop.perform([]() {
            });
        } };
        static_cast<void>(probe);

        static_cast<void>(dross::current_runloop());
    } };
    worker.join();

    ASSERT_TRUE(performed.has_value());
    EXPECT_FALSE(*performed);
}
