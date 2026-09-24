#include "dross/thread/runloop.h"
#include "dross/thread/thread.h"
#include "test_support.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <thread>
#include <vector>

namespace {

using dross_test::kTimeout;

}  // namespace

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

    ASSERT_TRUE(first.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(second.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(first.join_for(kTimeout));
    ASSERT_TRUE(second.join_for(kTimeout));
}

TEST(thread_test, a_worker_is_not_the_main_thread)
{
    // A handle to the main thread, captured here and compared against below.
    const dross::thread main_handle = dross::main_thread();

    dross::thread worker;
    std::atomic<bool> equal_to_main{ true };

    ASSERT_TRUE(worker.perform([main_handle, &equal_to_main]() {
        equal_to_main.store(dross::current_thread() == main_handle);
    }));

    ASSERT_TRUE(worker.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    EXPECT_FALSE(equal_to_main.load());
}

TEST(thread_test, is_current_thread_is_true_only_on_the_thread_it_names)
{
    dross::thread worker;
    std::atomic<bool> true_on_worker{ false };

    ASSERT_TRUE(worker.perform([&worker, &true_on_worker]() {
        true_on_worker.store(worker.is_current_thread());
    }));
    ASSERT_TRUE(worker.perform([]() {
        dross::current_thread().quit();
    }));
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

    ASSERT_TRUE(first.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(second.perform([]() {
        dross::current_thread().quit();
    }));
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

    ASSERT_TRUE(worker.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    ASSERT_TRUE(worker.native_id().has_value());
    EXPECT_EQ(*worker.native_id(), id_while_running);
}

TEST(thread_test, a_loop_thread_runs_a_task_on_its_own_thread)
{
    dross::thread worker;
    std::atomic<std::uint64_t> ran_on{ 0 };

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
    std::atomic<bool> ran{ false };
    dross::thread worker{ [&ran]() {
        ran.store(true);
    } };

    ASSERT_TRUE(worker.join_for(kTimeout));
    EXPECT_TRUE(ran.load());
    EXPECT_FALSE(worker.perform([]() {
    }));
}

TEST(thread_test, cancel_sets_stop_requested_and_a_polling_body_sees_it)
{
    dross::thread worker;
    std::atomic<bool> saw_stop{ false };

    ASSERT_TRUE(worker.perform([&saw_stop]() {
        while (! dross::current_thread().stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
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

    ASSERT_TRUE(worker.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    EXPECT_FALSE(worker.running());
    EXPECT_TRUE(worker.finished());
}

TEST(thread_test, join_for_zero_does_not_wait_and_reports_the_current_state)
{
    dross::thread worker;

    const auto started = std::chrono::steady_clock::now();
    const bool already_finished = worker.join_for(std::chrono::milliseconds{ 0 });
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_FALSE(already_finished);
    EXPECT_LT(elapsed, std::chrono::milliseconds{ 100 });

    ASSERT_TRUE(worker.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    EXPECT_TRUE(worker.join_for(std::chrono::milliseconds{ 0 }));
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

    ASSERT_TRUE(worker.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    const std::vector<dross::thread> threads = dross::all_threads();
    EXPECT_EQ(std::find(threads.begin(), threads.end(), worker), threads.end());
}

TEST(thread_test, a_handle_outliving_the_thread_is_still_usable)
{
    dross::thread worker;

    ASSERT_TRUE(worker.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(worker.join_for(kTimeout));

    EXPECT_TRUE(worker.finished());
    EXPECT_FALSE(worker.perform([]() {
    }));
}

TEST(thread_test, many_threads_drive_the_registry_and_the_loop_concurrently)
{
    constexpr int kWorkers = 8;
    constexpr int kTasksPerWorker = 25;

    std::vector<dross::thread> workers(kWorkers);
    std::atomic<int> ran{ 0 };

    std::vector<std::thread> posters;
    posters.reserve(kWorkers);
    for (auto& worker : workers) {
        posters.emplace_back([&worker, &ran]() {
            for (int n = 0; n < kTasksPerWorker; ++n) {
                worker.perform([&ran]() {
                    ran.fetch_add(1);
                });
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
        ASSERT_TRUE(worker.perform([]() {
            dross::current_thread().quit();
        }));
    }
    for (auto& worker : workers) {
        ASSERT_TRUE(worker.join_for(kTimeout));
    }

    EXPECT_EQ(ran.load(), kWorkers * kTasksPerWorker);
}

TEST(thread_test, join_blocks_until_another_thread_ends)
{
    dross::thread worker;
    std::atomic<bool> join_returned{ false };

    // The watcher's own join_for() below is what keeps this bounded even if
    // worker.join() itself never returns: a bare join() has no timeout by
    // design, so it is only ever called from a thread we can separately
    // bound our wait on.
    dross::thread watcher{ [worker, &join_returned]() mutable {
        worker.join();
        join_returned.store(true);
    } };

    std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });
    EXPECT_FALSE(join_returned.load());

    ASSERT_TRUE(worker.perform([]() {
        dross::current_thread().quit();
    }));

    ASSERT_TRUE(watcher.join_for(kTimeout));
    EXPECT_TRUE(join_returned.load());
}

TEST(thread_test, join_on_the_calling_threads_own_handle_returns_at_once)
{
    dross::thread worker;
    std::atomic<bool> returned_before_quit{ false };

    ASSERT_TRUE(worker.perform([&returned_before_quit]() {
        dross::current_thread().join();
        returned_before_quit.store(true);
        dross::current_thread().quit();
    }));

    ASSERT_TRUE(worker.join_for(kTimeout));
    EXPECT_TRUE(returned_before_quit.load());
}

TEST(thread_test, is_current_thread_and_join_work_on_a_record_reached_via_all_threads_without_adoption)
{
    // A worker asks about the main thread first, so the main thread's own
    // record gets registered without the main thread itself ever going
    // through current_thread_storage(). This is exactly how defect 1 arose:
    // a thread reaching its own record through all_threads() without ever
    // having been adopted.
    dross::thread finder{ []() {
        static_cast<void>(dross::main_thread());
    } };
    ASSERT_TRUE(finder.join_for(kTimeout));

    std::vector<dross::thread> threads = dross::all_threads();
    const auto it = std::find_if(threads.begin(), threads.end(), [](const dross::thread& candidate) {
        return candidate.is_current_thread();
    });

    // Gated behind this assertion rather than calling join() unconditionally
    // below: before the fix, is_current_thread() returned false here, and
    // join() would then have waited forever instead of returning at once.
    ASSERT_NE(it, threads.end());

    it->join();
}

TEST(thread_test, quit_can_be_called_from_a_different_thread)
{
    dross::thread worker;

    worker.quit();

    ASSERT_TRUE(worker.join_for(kTimeout));
}

TEST(thread_test, copy_assignment_names_the_same_thread)
{
    dross::thread first;
    dross::thread second;
    ASSERT_FALSE(first == second);

    second = first;
    EXPECT_TRUE(second == first);

    ASSERT_TRUE(first.perform([]() {
        dross::current_thread().quit();
    }));
    ASSERT_TRUE(first.join_for(kTimeout));
}

TEST(thread_test, an_adopted_thread_that_touched_the_loop_first_reports_perform_false_once_finished)
{
    std::optional<dross::thread> adopted;

    dross::thread worker{ [&adopted]() {
        // Touches the run loop module before this one, so the run loop's
        // own thread-local holder is constructed first and destroyed last
        // on this thread. This is exactly how defect 2 arose.
        static_cast<void>(dross::current_runloop());
        adopted = dross::current_thread();
    } };

    ASSERT_TRUE(worker.join_for(kTimeout));
    ASSERT_TRUE(adopted.has_value());

    EXPECT_TRUE(adopted->finished());
    EXPECT_FALSE(adopted->perform([]() {
    }));
}

TEST(thread_test, current_thread_is_defined_from_a_thread_local_destructor_that_outlives_the_holder)
{
    struct destructor_probe final {
        std::function<void()> on_destroy;
        ~destructor_probe()
        {
            on_destroy();
        }
    };

    std::optional<bool> performed;
    std::optional<bool> was_finished;

    // A plain std::thread, not dross::thread: starting one of those installs
    // this module's own thread-local holder before the body ever runs,
    // which would construct it before probe below rather than after.
    std::thread worker{ [&performed, &was_finished]() {
        // Constructed before this thread ever calls current_thread(), so it
        // is destroyed after that thread's own holder: the same ordering
        // that makes a user's own thread-local destructor run after this
        // library's when the user's was constructed first.
        thread_local destructor_probe probe{ [&performed, &was_finished]() {
            dross::thread self = dross::current_thread();
            performed = self.perform([]() {
            });
            was_finished = self.finished();
        } };
        static_cast<void>(probe);

        static_cast<void>(dross::current_thread());
    } };
    worker.join();

    ASSERT_TRUE(performed.has_value());
    ASSERT_TRUE(was_finished.has_value());
    EXPECT_FALSE(*performed);
    EXPECT_TRUE(*was_finished);
}

TEST(thread_test, a_worker_asking_for_main_thread_first_still_reaches_the_real_main_loop)
{
    // The main thread has not used the module yet in this process; a worker
    // is the first to ask for main_thread(). native::main_thread_id() and
    // main_runloop() are both defined to answer correctly regardless of
    // which thread asks, so the task below must still land on the real main
    // thread's loop, not on whichever thread happens to build the record.
    std::atomic<bool> queued{ false };
    dross::thread finder{ [&queued]() {
        dross::thread main_handle = dross::main_thread();
        queued.store(main_handle.perform([]() {
            dross::main_runloop().quit();
        }));
    } };
    ASSERT_TRUE(finder.join_for(kTimeout));
    ASSERT_TRUE(queued.load());

    // Run on the real main thread. If the task above had landed on the
    // wrong loop, nothing would ever call quit() here, and this would run
    // out its whole timeout instead of returning early with one task run.
    EXPECT_EQ(dross::main_runloop().run_for(kTimeout), 1U);
}

TEST(thread_test, join_for_with_a_huge_timeout_does_not_overflow)
{
    dross::thread worker;

    std::thread delayed_quit{ [worker]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });
        worker.quit();
    } };

    // Definitely still running when this is called, since the quit above is
    // delayed: an overflowed deadline would make this return false at once
    // instead of waiting the short while for that quit() to land.
    const auto started = std::chrono::steady_clock::now();
    EXPECT_TRUE(worker.join_for(std::chrono::milliseconds::max()));
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_GE(elapsed, std::chrono::milliseconds{ 40 });
    EXPECT_LT(elapsed, kTimeout);

    delayed_quit.join();
}
