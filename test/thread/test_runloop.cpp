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
    loop.run_for(std::chrono::milliseconds{0});
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

TEST(runloop_test, a_worker_hands_work_back_to_the_main_thread)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    std::atomic<std::thread::id> ran_on{};
    std::thread worker{[loop, &ran_on]() mutable {
        loop.perform([&ran_on]() { ran_on.store(std::this_thread::get_id()); });
    }};

    EXPECT_TRUE(loop.run_one());
    EXPECT_EQ(ran_on.load(), std::this_thread::get_id());

    worker.join();
}

TEST(runloop_test, tasks_from_many_threads_all_arrive)
{
    reset_main_runloop();
    dross::runloop loop = dross::main_runloop();

    constexpr int kPosters = 4;
    constexpr int kPerPoster = 50;

    std::atomic<int> ran{0};
    std::vector<std::thread> posters;
    posters.reserve(kPosters);
    for (int poster = 0; poster < kPosters; ++poster) {
        posters.emplace_back([loop, &ran]() mutable {
            for (int n = 0; n < kPerPoster; ++n) {
                loop.perform([&ran]() { ran.fetch_add(1); });
            }
        });
    }

    for (int seen = 0; seen < kPosters * kPerPoster; ++seen) {
        ASSERT_TRUE(loop.run_one());
    }

    for (auto& poster : posters) {
        poster.join();
    }

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
