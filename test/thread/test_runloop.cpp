#include <gtest/gtest.h>

#include "dross/thread/runloop.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

// Every test here shares the main thread's loop, so the ones that use it
// start from a known state: no queued tasks, no quit request left behind.
void reset_main_runloop()
{
    dross::runloop loop = dross::main_runloop();
    loop.clear();
    loop.run_for(std::chrono::milliseconds{1});
}

}

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
    std::thread worker{[&theirs]() { theirs = dross::current_runloop(); }};
    worker.join();

    ASSERT_TRUE(theirs.has_value());
    EXPECT_FALSE(*theirs == mine);
}

TEST(runloop_test, a_worker_reaches_the_same_main_loop)
{
    const dross::runloop here = dross::main_runloop();

    std::optional<dross::runloop> there;
    std::thread worker{[&there]() { there = dross::main_runloop(); }};
    worker.join();

    ASSERT_TRUE(there.has_value());
    EXPECT_TRUE(*there == here);
}

TEST(runloop_test, perform_queues_without_running)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    EXPECT_TRUE(loop.empty());
    EXPECT_TRUE(loop.perform([]() {}));
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
    std::thread worker{[&theirs]() { theirs = dross::current_runloop(); }};
    worker.join();

    ASSERT_TRUE(theirs.has_value());
    EXPECT_FALSE(theirs->perform([]() {}));
}

TEST(runloop_test, run_pending_runs_what_is_queued_in_order)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    std::vector<int> order;
    loop.perform([&order]() { order.push_back(1); });
    loop.perform([&order]() { order.push_back(2); });

    EXPECT_EQ(loop.run_pending(), 2U);
    EXPECT_EQ(order, (std::vector<int>{1, 2}));
    EXPECT_TRUE(loop.empty());
}

TEST(runloop_test, run_pending_leaves_a_task_that_a_task_added)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([loop]() mutable { loop.perform([]() {}); });

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
        loop.perform([&c_ran]() { c_ran = true; });
    });
    loop.perform([]() {});

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
    loop.perform([&ran]() { ran = true; });
    loop.clear();

    EXPECT_EQ(loop.run_pending(), 0U);
    EXPECT_FALSE(ran);
}

TEST(runloop_test, quit_ends_run_and_keeps_the_queue)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([]() {});
    loop.quit();

    EXPECT_EQ(loop.run(), 0U);
    EXPECT_EQ(loop.pending_count(), 1U);

    loop.clear();
}

TEST(runloop_test, run_pending_does_not_consume_a_pending_quit)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.quit();

    EXPECT_EQ(loop.run_pending(), 0U);

    // A surviving quit makes run_for return at once; a consumed one makes it
    // wait out the full timeout.
    const auto started = std::chrono::steady_clock::now();
    EXPECT_EQ(loop.run_for(std::chrono::milliseconds{200}), 0U);
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::milliseconds{100});
}

TEST(runloop_test, a_quit_from_another_thread_ends_a_waiting_run)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    std::thread worker{[loop]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        loop.quit();
    }};

    EXPECT_EQ(loop.run(), 0U);
    worker.join();
}

TEST(runloop_test, run_returns_the_number_of_tasks_it_ran)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([]() {});
    loop.perform([]() {});
    loop.perform([loop]() mutable { loop.quit(); });

    EXPECT_EQ(loop.run(), 3U);
}

TEST(runloop_test, run_one_returns_false_when_quit_is_pending)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([]() {});
    loop.quit();

    EXPECT_FALSE(loop.run_one());

    loop.clear();
}

TEST(runloop_test, a_nested_run_one_does_not_consume_the_outer_quit)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([loop]() mutable {
        loop.quit();
        loop.run_one();
    });

    // A watchdog in case of a regression: without the fix, the outer run()
    // below hangs because the nested run_one() above already consumed the
    // quit it was not meant to see.
    std::atomic<bool> outer_done{false};
    std::thread watchdog{[loop, &outer_done]() mutable {
        std::this_thread::sleep_for(std::chrono::seconds{2});
        if (!outer_done.load()) {
            loop.quit();
        }
    }};

    const auto started = std::chrono::steady_clock::now();
    loop.run();
    const auto elapsed = std::chrono::steady_clock::now() - started;
    outer_done.store(true);

    watchdog.join();

    EXPECT_LT(elapsed, std::chrono::milliseconds{500});
}

TEST(runloop_test, a_worker_hands_work_back_to_the_main_thread)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    std::atomic<std::thread::id> ran_on{};
    std::atomic<bool> queued{false};
    std::thread worker{[loop, &ran_on, &queued]() mutable {
        queued.store(loop.perform([loop, &ran_on]() mutable {
            ran_on.store(std::this_thread::get_id());
            loop.quit();
        }));
    }};

    EXPECT_EQ(loop.run_for(std::chrono::seconds{5}), 1U);
    EXPECT_EQ(ran_on.load(), std::this_thread::get_id());

    worker.join();
    EXPECT_TRUE(queued.load());
}

TEST(runloop_test, tasks_from_many_threads_all_arrive)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    constexpr int kPosters = 4;
    constexpr int kPerPoster = 50;

    std::atomic<int> ran{0};
    std::atomic<int> queued{0};
    std::vector<std::thread> posters;
    posters.reserve(kPosters);
    for (int poster = 0; poster < kPosters; ++poster) {
        posters.emplace_back([loop, &ran, &queued]() mutable {
            for (int n = 0; n < kPerPoster; ++n) {
                if (loop.perform([&ran]() { ran.fetch_add(1); })) {
                    queued.fetch_add(1);
                }
            }
        });
    }

    std::size_t seen = 0;
    while (seen < static_cast<std::size_t>(kPosters * kPerPoster)) {
        const std::size_t n = loop.run_for(std::chrono::milliseconds{100});
        ASSERT_GT(n, 0U);
        seen += n;
    }

    for (auto& poster : posters) {
        poster.join();
    }

    EXPECT_EQ(queued.load(), kPosters * kPerPoster);
    EXPECT_EQ(ran.load(), kPosters * kPerPoster);
    EXPECT_TRUE(loop.empty());
}

TEST(runloop_test, run_for_returns_when_the_timeout_passes)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    const auto started = std::chrono::steady_clock::now();
    EXPECT_EQ(loop.run_for(std::chrono::milliseconds{20}), 0U);
    EXPECT_GE(std::chrono::steady_clock::now() - started, std::chrono::milliseconds{15});
}

TEST(runloop_test, run_for_runs_what_is_queued)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([]() {});
    loop.perform([]() {});

    EXPECT_EQ(loop.run_for(std::chrono::milliseconds{20}), 2U);
}

TEST(runloop_test, run_for_zero_runs_nothing)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([]() {});

    EXPECT_EQ(loop.run_for(std::chrono::milliseconds{0}), 0U);
    EXPECT_EQ(loop.pending_count(), 1U);

    loop.clear();
}

TEST(runloop_test, run_for_with_a_huge_timeout_does_not_overflow)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([loop]() mutable { loop.quit(); });

    const auto started = std::chrono::steady_clock::now();
    EXPECT_EQ(loop.run_for(std::chrono::milliseconds::max()), 1U);
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::milliseconds{500});
}

TEST(runloop_test, is_running_is_true_inside_a_task)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    bool seen = false;
    loop.perform([loop, &seen]() mutable { seen = loop.is_running(); });

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

    loop.perform([]() { throw std::runtime_error{"from a task"}; });
    loop.perform([]() {});

    EXPECT_THROW(loop.run_pending(), std::runtime_error);
    EXPECT_EQ(loop.pending_count(), 1U);
    EXPECT_FALSE(loop.is_running());

    loop.clear();
}

TEST(runloop_test, an_exception_from_a_task_propagates_out_of_run)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    loop.perform([]() { throw std::runtime_error{"from a task"}; });

    EXPECT_THROW(loop.run(), std::runtime_error);
    EXPECT_FALSE(loop.is_running());

    bool ran = false;
    loop.perform([&ran]() { ran = true; });
    EXPECT_EQ(loop.run_pending(), 1U);
    EXPECT_TRUE(ran);
}
