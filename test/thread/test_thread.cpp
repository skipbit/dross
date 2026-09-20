#include <gtest/gtest.h>

#include "dross/thread/thread.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

// Generous enough that a real regression fails instead of flaking, but short
// enough that a genuine hang does not stall the suite.
constexpr auto kTimeout = std::chrono::seconds{5};

}

TEST(thread_test, two_handles_from_the_main_thread_name_the_same_thread)
{
    EXPECT_TRUE(dross::current_thread() == dross::current_thread());
}

TEST(thread_test, a_copied_handle_names_the_same_thread)
{
    const dross::thread original = dross::current_thread();
    const dross::thread copy = original;

    EXPECT_TRUE(copy == original);
}

TEST(thread_test, the_main_threads_current_thread_is_the_main_thread)
{
    EXPECT_TRUE(dross::current_thread() == dross::main_thread());
}

TEST(thread_test, two_different_threads_are_not_equal)
{
    dross::thread first;
    dross::thread second;

    EXPECT_FALSE(first == second);

    ASSERT_TRUE(first.perform([]() { dross::current_thread().quit(); }));
    ASSERT_TRUE(second.perform([]() { dross::current_thread().quit(); }));
    ASSERT_TRUE(first.join_for(kTimeout));
    ASSERT_TRUE(second.join_for(kTimeout));
}

TEST(thread_test, a_worker_is_not_the_main_thread)
{
    // Established from the main thread first: main_thread() lazily adopts
    // whichever thread first asks about it, so a worker must not be the one
    // to ask first.
    const dross::thread main_handle = dross::main_thread();

    dross::thread worker;
    std::atomic<bool> equal_to_main{true};

    ASSERT_TRUE(worker.perform([main_handle, &equal_to_main]() {
        equal_to_main.store(dross::current_thread() == main_handle);
    }));

    ASSERT_TRUE(worker.perform([]() { dross::current_thread().quit(); }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    EXPECT_FALSE(equal_to_main.load());
}

TEST(thread_test, is_current_thread_is_true_only_on_the_thread_it_names)
{
    dross::thread worker;
    std::atomic<bool> true_on_worker{false};

    ASSERT_TRUE(worker.perform([&worker, &true_on_worker]() {
        true_on_worker.store(worker.is_current_thread());
    }));
    ASSERT_TRUE(worker.perform([]() { dross::current_thread().quit(); }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    EXPECT_TRUE(true_on_worker.load());
    EXPECT_FALSE(worker.is_current_thread());
}

TEST(thread_test, native_id_differs_between_two_live_threads_and_is_always_engaged)
{
    // The test target only sees public headers, so dross::native::thread_id()
    // is not reachable here to compare against directly.
    dross::thread first;
    dross::thread second;

    ASSERT_TRUE(first.native_id().has_value());
    ASSERT_TRUE(second.native_id().has_value());
    EXPECT_NE(*first.native_id(), *second.native_id());

    ASSERT_TRUE(first.perform([]() { dross::current_thread().quit(); }));
    ASSERT_TRUE(second.perform([]() { dross::current_thread().quit(); }));
    ASSERT_TRUE(first.join_for(kTimeout));
    ASSERT_TRUE(second.join_for(kTimeout));
}

TEST(thread_test, main_threads_native_id_is_known_once_the_main_thread_has_used_the_module)
{
#if defined(__linux__)
    // native::main_thread_id() reads this from any thread on Linux, so it is
    // known even before the main thread has touched the module itself.
    EXPECT_TRUE(dross::main_thread().native_id().has_value());
#else
    // Elsewhere it can only be read on the main thread itself; adopting it
    // is what fills the id in.
    static_cast<void>(dross::current_thread());
    EXPECT_TRUE(dross::main_thread().native_id().has_value());
#endif
}

TEST(thread_test, a_handle_to_an_ended_thread_still_reports_its_id)
{
    dross::thread worker;
    ASSERT_TRUE(worker.native_id().has_value());
    const std::uint64_t id_while_running = *worker.native_id();

    ASSERT_TRUE(worker.perform([]() { dross::current_thread().quit(); }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    ASSERT_TRUE(worker.native_id().has_value());
    EXPECT_EQ(*worker.native_id(), id_while_running);
}

TEST(thread_test, a_loop_thread_runs_a_task_on_its_own_thread)
{
    dross::thread worker;
    std::atomic<std::uint64_t> ran_on{0};

    ASSERT_TRUE(worker.perform([&ran_on]() {
        ran_on.store(dross::current_thread().native_id().value());
        dross::current_thread().quit();
    }));

    ASSERT_TRUE(worker.join_for(kTimeout));
    ASSERT_TRUE(worker.native_id().has_value());
    EXPECT_EQ(ran_on.load(), *worker.native_id());
}

TEST(thread_test, a_one_shot_thread_runs_its_body_and_ends)
{
    std::atomic<bool> ran{false};
    dross::thread worker{[&ran]() { ran.store(true); }};

    ASSERT_TRUE(worker.join_for(kTimeout));
    EXPECT_TRUE(ran.load());
    EXPECT_FALSE(worker.perform([]() {}));
}

TEST(thread_test, cancel_sets_stop_requested_and_a_polling_body_sees_it)
{
    dross::thread worker;
    std::atomic<bool> saw_stop{false};

    ASSERT_TRUE(worker.perform([&saw_stop]() {
        while (!dross::current_thread().stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        saw_stop.store(true);
        dross::current_thread().quit();
    }));

    EXPECT_FALSE(worker.stop_requested());
    worker.cancel();
    EXPECT_TRUE(worker.stop_requested());

    ASSERT_TRUE(worker.join_for(kTimeout));
    EXPECT_TRUE(saw_stop.load());
}

TEST(thread_test, finished_and_running_reflect_the_threads_lifetime)
{
    dross::thread worker;

    EXPECT_TRUE(worker.running());
    EXPECT_FALSE(worker.finished());

    ASSERT_TRUE(worker.perform([]() { dross::current_thread().quit(); }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    EXPECT_FALSE(worker.running());
    EXPECT_TRUE(worker.finished());
}

TEST(thread_test, join_for_zero_does_not_wait_and_reports_the_current_state)
{
    dross::thread worker;

    const auto started = std::chrono::steady_clock::now();
    const bool already_finished = worker.join_for(std::chrono::milliseconds{0});
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_FALSE(already_finished);
    EXPECT_LT(elapsed, std::chrono::milliseconds{100});

    ASSERT_TRUE(worker.perform([]() { dross::current_thread().quit(); }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    EXPECT_TRUE(worker.join_for(std::chrono::milliseconds{0}));
}

TEST(thread_test, all_threads_includes_the_main_thread)
{
    // Registration happens when a thread is first named; ask for the main
    // thread before snapshotting the registry, not after.
    const dross::thread main_handle = dross::main_thread();
    const std::vector<dross::thread> threads = dross::all_threads();

    EXPECT_NE(std::find(threads.begin(), threads.end(), main_handle), threads.end());
}

TEST(thread_test, all_threads_gains_and_loses_a_started_thread)
{
    dross::thread worker;

    {
        const std::vector<dross::thread> threads = dross::all_threads();
        EXPECT_NE(std::find(threads.begin(), threads.end(), worker), threads.end());
    }

    ASSERT_TRUE(worker.perform([]() { dross::current_thread().quit(); }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    const std::vector<dross::thread> threads = dross::all_threads();
    EXPECT_EQ(std::find(threads.begin(), threads.end(), worker), threads.end());
}

TEST(thread_test, a_handle_outliving_the_thread_is_still_usable)
{
    dross::thread worker;

    ASSERT_TRUE(worker.perform([]() { dross::current_thread().quit(); }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    EXPECT_TRUE(worker.finished());
    EXPECT_FALSE(worker.perform([]() {}));
}

TEST(thread_test, many_threads_drive_the_registry_and_the_loop_concurrently)
{
    constexpr int kWorkers = 8;
    constexpr int kTasksPerWorker = 25;

    std::vector<dross::thread> workers(kWorkers);
    std::atomic<int> ran{0};

    std::vector<std::thread> posters;
    posters.reserve(kWorkers);
    for (auto& worker : workers) {
        posters.emplace_back([&worker, &ran]() {
            for (int n = 0; n < kTasksPerWorker; ++n) {
                worker.perform([&ran]() { ran.fetch_add(1); });
            }
            // Exercised concurrently with the perform() calls above, on a
            // thread of its own: adopts this poster and reads the registry
            // while other posters and worker loops are doing the same.
            static_cast<void>(dross::all_threads());
            static_cast<void>(dross::current_thread());
        });
    }
    for (auto& poster : posters) {
        poster.join();
    }

    for (auto& worker : workers) {
        ASSERT_TRUE(worker.perform([]() { dross::current_thread().quit(); }));
    }
    for (auto& worker : workers) {
        ASSERT_TRUE(worker.join_for(kTimeout));
    }

    EXPECT_EQ(ran.load(), kWorkers * kTasksPerWorker);
}
