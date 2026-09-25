#include "dross/thread/operation_queue.h"
#include "dross/thread/runloop.h"
#include "dross/thread/thread.h"
#include "dross/thread/timer.h"
#include "observed_time_source.h"
#include "test_support.h"
#include "thread/operation_queue_access.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

using dross_test::event;
using dross_test::kTimeout;

// Holds each task that arrives until count of them have, so count tasks that
// all get through were running at once, each on a worker of its own. Keeps
// the thread each one ran on.
class gathering final {
public:
    explicit gathering(std::size_t count)
        : _count{ count }
    {
    }

    void arrive()
    {
        std::unique_lock<std::mutex> lock{ _mutex };
        _threads.push_back(dross::current_thread());
        _wake.notify_all();
        _wake.wait_for(lock, kTimeout, [this]() {
            return (_threads.size() >= _count);
        });
    }

    // The threads, or fewer than count if bound passed before they all
    // arrived.
    std::vector<dross::thread> await(std::chrono::milliseconds bound = kTimeout)
    {
        std::unique_lock<std::mutex> lock{ _mutex };
        _wake.wait_for(lock, bound, [this]() {
            return (_threads.size() >= _count);
        });
        return _threads;
    }

private:
    const std::size_t _count;
    std::mutex _mutex;
    std::condition_variable _wake;
    std::vector<dross::thread> _threads;
};

// Gathers one task on each worker and returns the workers, or fewer when
// they did not all start.
std::vector<dross::thread> workers_of(dross::operation_queue& queue)
{
    auto gather = std::make_shared<gathering>(queue.thread_count());
    for (std::size_t n = 0; n < queue.thread_count(); ++n) {
        queue.submit([gather]() {
            gather->arrive();
        });
    }
    return gather->await();
}

bool all_end(std::vector<dross::thread>& workers)
{
    for (auto& worker : workers) {
        if (! worker.join_for(kTimeout)) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST(operation_queue_test, zero_workers_is_rejected)
{
    EXPECT_THROW(dross::operation_queue{ 0 }, std::invalid_argument);
}

TEST(operation_queue_test, thread_count_is_the_count_it_was_made_with)
{
    const dross::operation_queue queue{ 3 };

    EXPECT_EQ(queue.thread_count(), 3U);
}

TEST(operation_queue_test, a_copied_handle_names_the_same_queue)
{
    const dross::operation_queue original{ 1 };
    const dross::operation_queue copy = original;

    EXPECT_TRUE(copy == original);
}

TEST(operation_queue_test, two_different_queues_are_not_equal)
{
    const dross::operation_queue first{ 1 };
    const dross::operation_queue second{ 1 };

    EXPECT_FALSE(first == second);
}

TEST(operation_queue_test, an_empty_task_is_not_queued)
{
    dross::operation_queue queue{ 1 };

    EXPECT_FALSE(queue.submit(nullptr));
}

TEST(operation_queue_test, every_worker_runs_a_task_at_once)
{
    dross::operation_queue queue{ 3 };

    auto workers = workers_of(queue);

    ASSERT_EQ(workers.size(), 3U);
    EXPECT_FALSE(workers[0] == workers[1]);
    EXPECT_FALSE(workers[0] == workers[2]);
    EXPECT_FALSE(workers[1] == workers[2]);
    for (const auto& worker : workers) {
        EXPECT_FALSE(worker == dross::current_thread());
    }
}

TEST(operation_queue_test, a_worker_busy_with_its_own_loop_does_not_hold_up_a_task)
{
    dross::operation_queue queue{ 2 };
    const auto workers = workers_of(queue);
    ASSERT_EQ(workers.size(), 2U);

    // Busies each worker in turn, so whichever one submit() would try first
    // is busy in one of the rounds.
    for (const auto& busy : workers) {
        struct state {
            event queued;
            event release;
            event ran;
        };
        auto shared = std::make_shared<state>();
        const auto gather = std::make_shared<gathering>(2);
        for (int n = 0; n < 2; ++n) {
            ASSERT_TRUE(queue.submit([busy, gather, shared]() {
                gather->arrive();
                if (busy == dross::current_thread()) {
                    dross::current_thread().perform([shared]() {
                        shared->release.wait();
                    });
                    shared->queued.set();
                }
            }));
        }
        ASSERT_TRUE(shared->queued.wait());
        ASSERT_TRUE(queue.wait_for(kTimeout));

        ASSERT_TRUE(queue.submit([shared]() {
            shared->ran.set();
        }));
        const bool ran = shared->ran.wait();
        shared->release.set();

        EXPECT_TRUE(ran);
    }
}

TEST(operation_queue_test, tasks_start_in_the_order_they_were_submitted)
{
    struct state {
        event release;
        event done;
        std::mutex mutex;
        std::vector<int> order;
    };
    auto shared = std::make_shared<state>();
    dross::operation_queue queue{ 1 };

    // The first holds the only worker until every task is queued behind it.
    ASSERT_TRUE(queue.submit([shared]() {
        shared->release.wait();
    }));
    for (int n = 0; n < 5; ++n) {
        ASSERT_TRUE(queue.submit([shared, n]() {
            const std::lock_guard<std::mutex> guard{ shared->mutex };
            shared->order.push_back(n);
        }));
    }
    ASSERT_TRUE(queue.submit([shared]() {
        shared->done.set();
    }));
    shared->release.set();

    ASSERT_TRUE(shared->done.wait());
    const std::lock_guard<std::mutex> guard{ shared->mutex };
    EXPECT_EQ(shared->order, (std::vector<int>{ 0, 1, 2, 3, 4 }));
}

TEST(operation_queue_test, tasks_from_many_threads_all_run)
{
    constexpr int kSubmitters = 4;
    constexpr int kPerSubmitter = 50;
    constexpr int kTotal = kSubmitters * kPerSubmitter;

    struct state {
        std::atomic<int> ran{ 0 };
        std::atomic<int> queued{ 0 };
        event all_ran;
    };
    auto shared = std::make_shared<state>();
    dross::operation_queue queue{ 3 };

    std::vector<std::thread> submitters;
    submitters.reserve(kSubmitters);
    for (int submitter = 0; submitter < kSubmitters; ++submitter) {
        submitters.emplace_back([queue, shared]() mutable {
            for (int n = 0; n < kPerSubmitter; ++n) {
                if (queue.submit([shared]() {
                    if (shared->ran.fetch_add(1) + 1 == kTotal) {
                        shared->all_ran.set();
                    }
                })) {
                    shared->queued.fetch_add(1);
                }
            }
        });
    }
    for (auto& submitter : submitters) {
        submitter.join();
    }

    EXPECT_EQ(shared->queued.load(), kTotal);
    EXPECT_TRUE(shared->all_ran.wait());
    EXPECT_EQ(shared->ran.load(), kTotal);
}

TEST(operation_queue_test, a_task_can_perform_on_its_worker_and_install_a_timer_there)
{
    struct state {
        event performed;
        event fired;
        std::atomic<bool> performed_on_the_worker{ false };
        std::atomic<bool> fired_on_the_worker{ false };
    };
    auto shared = std::make_shared<state>();
    dross::operation_queue queue{ 2 };

    ASSERT_TRUE(queue.submit([shared]() {
        const dross::thread worker = dross::current_thread();
        dross::current_thread().perform([shared, worker]() {
            shared->performed_on_the_worker.store(worker == dross::current_thread());
            shared->performed.set();
        });
        dross::timer::once(std::chrono::milliseconds{ 1 }, [shared, worker](dross::timer) {
            shared->fired_on_the_worker.store(worker == dross::current_thread());
            shared->fired.set();
        });
    }));

    ASSERT_TRUE(shared->performed.wait());
    ASSERT_TRUE(shared->fired.wait());
    EXPECT_TRUE(shared->performed_on_the_worker.load());
    EXPECT_TRUE(shared->fired_on_the_worker.load());
}

TEST(operation_queue_test, the_last_handle_going_ends_the_workers)
{
    std::vector<dross::thread> workers;
    {
        dross::operation_queue queue{ 2 };
        workers = workers_of(queue);
        ASSERT_EQ(workers.size(), 2U);
    }

    EXPECT_TRUE(all_end(workers));
}

TEST(operation_queue_test, a_copy_keeps_the_queue_accepting_after_the_original_goes)
{
    std::optional<dross::operation_queue> original{ std::in_place, 1 };
    dross::operation_queue copy = *original;
    auto workers = workers_of(copy);
    ASSERT_EQ(workers.size(), 1U);

    original.reset();

    auto ran = std::make_shared<event>();
    ASSERT_TRUE(copy.submit([ran]() {
        ran->set();
    }));
    EXPECT_TRUE(ran->wait());
    EXPECT_FALSE(workers[0].finished());
}

TEST(operation_queue_test, tasks_left_when_the_last_handle_goes_still_run)
{
    struct state {
        event release;
        std::atomic<int> ran{ 0 };
    };
    auto shared = std::make_shared<state>();
    std::vector<dross::thread> workers;
    {
        dross::operation_queue queue{ 1 };
        workers = workers_of(queue);
        ASSERT_EQ(workers.size(), 1U);

        ASSERT_TRUE(queue.submit([shared]() {
            shared->release.wait();
        }));
        for (int n = 0; n < 3; ++n) {
            ASSERT_TRUE(queue.submit([shared]() {
                shared->ran.fetch_add(1);
            }));
        }
    }
    shared->release.set();

    ASSERT_TRUE(all_end(workers));
    EXPECT_EQ(shared->ran.load(), 3);
}

TEST(operation_queue_test, a_worker_runs_what_it_queued_on_its_own_loop_before_it_ends)
{
    struct state {
        event release;
        std::atomic<bool> ran{ false };
    };
    auto shared = std::make_shared<state>();
    std::vector<dross::thread> workers;
    {
        dross::operation_queue queue{ 1 };
        workers = workers_of(queue);
        ASSERT_EQ(workers.size(), 1U);

        ASSERT_TRUE(queue.submit([shared]() {
            shared->release.wait();
            dross::current_thread().perform([shared]() {
                shared->ran.store(true);
            });
        }));
    }
    shared->release.set();

    ASSERT_TRUE(all_end(workers));
    EXPECT_TRUE(shared->ran.load());
}

TEST(operation_queue_test, the_last_handle_going_on_its_own_worker_ends_that_worker)
{
    // The only handle, which the queue's own task drops on the worker, so
    // the worker must not wait for itself to end.
    auto holder = std::make_shared<std::optional<dross::operation_queue>>(std::in_place, 1);
    auto workers = workers_of(**holder);
    ASSERT_EQ(workers.size(), 1U);

    ASSERT_TRUE((*holder)->submit([holder]() {
        holder->reset();
    }));
    holder.reset();

    EXPECT_TRUE(all_end(workers));
}

TEST(operation_queue_test, wait_for_returns_once_the_submitted_tasks_have_finished)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    dross::operation_queue queue = dross::operation_queue_access::make(2, source);
    auto release = std::make_shared<event>();
    ASSERT_TRUE(queue.submit([release]() {
        release->wait();
    }));

    // The waiter's own bound is longer than the test waits for it, so only
    // the task finishing, not its deadline, ends the wait in time.
    std::atomic<bool> finished{ false };
    event returned;
    std::thread waiter{ [queue, &finished, &returned]() mutable {
        finished.store(queue.wait_for(kTimeout * 2));
        returned.set();
    } };
    // Released only once the waiter is waiting, so the task finishing is
    // what wakes it.
    const bool saw_wait = source->await_timed_waits(1);
    release->set();
    const bool woke = returned.wait();
    waiter.join();

    EXPECT_TRUE(saw_wait);
    EXPECT_TRUE(woke);
    EXPECT_TRUE(finished.load());
}

TEST(operation_queue_test, wait_for_does_not_wait_for_tasks_submitted_after_it_starts)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    dross::operation_queue queue = dross::operation_queue_access::make(2, source);
    auto first = std::make_shared<event>();
    auto later = std::make_shared<event>();
    ASSERT_TRUE(queue.submit([first]() {
        first->wait();
    }));

    std::atomic<bool> finished{ false };
    std::thread waiter{ [queue, &finished]() mutable {
        finished.store(queue.wait_for(kTimeout));
    } };
    const bool saw_wait = source->await_timed_waits(1);
    ASSERT_TRUE(queue.submit([later]() {
        later->wait();
    }));
    first->set();
    waiter.join();
    later->set();

    EXPECT_TRUE(saw_wait);
    EXPECT_TRUE(finished.load());
}

TEST(operation_queue_test, wait_for_times_out_while_a_task_is_still_running)
{
    dross::operation_queue queue{ 1 };
    auto release = std::make_shared<event>();
    ASSERT_TRUE(queue.submit([release]() {
        release->wait();
    }));

    EXPECT_FALSE(queue.wait_for(std::chrono::milliseconds{ 1 }));

    release->set();
    EXPECT_TRUE(queue.wait_for(kTimeout));
}

TEST(operation_queue_test, wait_for_zero_does_not_wait_and_reports_the_current_state)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    dross::operation_queue queue = dross::operation_queue_access::make(1, source);
    auto release = std::make_shared<event>();
    ASSERT_TRUE(queue.submit([release]() {
        release->wait();
    }));

    const bool while_running = queue.wait_for(std::chrono::milliseconds::zero());
    release->set();
    ASSERT_TRUE(queue.wait_for(kTimeout));
    const std::size_t waits_before = source->waits();
    const bool once_finished = queue.wait_for(std::chrono::milliseconds::zero());

    EXPECT_FALSE(while_running);
    EXPECT_TRUE(once_finished);
    EXPECT_EQ(source->waits(), waits_before);
}

TEST(operation_queue_test, wait_for_on_one_of_its_own_workers_does_not_wait)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    dross::operation_queue queue = dross::operation_queue_access::make(1, source);
    struct state {
        std::atomic<bool> finished{ true };
        event returned;
    };
    auto shared = std::make_shared<state>();

    ASSERT_TRUE(queue.submit([queue, shared]() mutable {
        shared->finished.store(queue.wait_for(kTimeout));
        shared->returned.set();
    }));

    ASSERT_TRUE(shared->returned.wait());
    EXPECT_FALSE(shared->finished.load());
    EXPECT_EQ(source->waits(), 0U);
}

TEST(operation_queue_test, shutdown_stops_accepting_and_ends_the_workers)
{
    dross::operation_queue queue{ 2 };
    auto workers = workers_of(queue);
    ASSERT_EQ(workers.size(), 2U);

    EXPECT_TRUE(queue.shutdown(kTimeout));
    EXPECT_FALSE(queue.submit([]() {
    }));
    for (const auto& worker : workers) {
        EXPECT_TRUE(worker.finished());
    }
}

TEST(operation_queue_test, shutdown_waits_for_the_tasks_already_submitted)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    dross::operation_queue queue = dross::operation_queue_access::make(1, source);
    struct state {
        event release;
        std::atomic<int> ran{ 0 };
    };
    auto shared = std::make_shared<state>();
    ASSERT_TRUE(queue.submit([shared]() {
        shared->release.wait();
    }));
    for (int n = 0; n < 3; ++n) {
        ASSERT_TRUE(queue.submit([shared]() {
            shared->ran.fetch_add(1);
        }));
    }

    // The same bounds as in wait_for's test: only the workers ending, not
    // the shutdown's own deadline, ends the wait in time.
    std::atomic<bool> ended{ false };
    event returned;
    std::thread stopper{ [queue, &ended, &returned]() mutable {
        ended.store(queue.shutdown(kTimeout * 2));
        returned.set();
    } };
    const bool saw_wait = source->await_timed_waits(1);
    shared->release.set();
    const bool woke = returned.wait();
    stopper.join();

    EXPECT_TRUE(saw_wait);
    EXPECT_TRUE(woke);
    EXPECT_TRUE(ended.load());
    EXPECT_EQ(shared->ran.load(), 3);
}

TEST(operation_queue_test, shutdown_times_out_while_a_task_is_still_running)
{
    dross::operation_queue queue{ 1 };
    auto release = std::make_shared<event>();
    ASSERT_TRUE(queue.submit([release]() {
        release->wait();
    }));

    EXPECT_FALSE(queue.shutdown(std::chrono::milliseconds{ 1 }));
    EXPECT_FALSE(queue.submit([]() {
    }));

    release->set();
    EXPECT_TRUE(queue.shutdown(kTimeout));
}

TEST(operation_queue_test, shutdown_zero_does_not_wait_and_reports_the_current_state)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    dross::operation_queue queue = dross::operation_queue_access::make(1, source);
    auto release = std::make_shared<event>();
    ASSERT_TRUE(queue.submit([release]() {
        release->wait();
    }));

    const bool while_running = queue.shutdown(std::chrono::milliseconds::zero());
    release->set();
    ASSERT_TRUE(queue.shutdown(kTimeout));
    const std::size_t waits_before = source->waits();
    const bool once_ended = queue.shutdown(std::chrono::milliseconds::zero());

    EXPECT_FALSE(while_running);
    EXPECT_TRUE(once_ended);
    EXPECT_EQ(source->waits(), waits_before);
}

TEST(operation_queue_test, shutdown_on_one_of_its_own_workers_does_not_wait)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    dross::operation_queue queue = dross::operation_queue_access::make(2, source);
    auto workers = workers_of(queue);
    ASSERT_EQ(workers.size(), 2U);
    struct state {
        std::atomic<bool> ended{ true };
        event returned;
    };
    auto shared = std::make_shared<state>();

    ASSERT_TRUE(queue.submit([queue, shared]() mutable {
        shared->ended.store(queue.shutdown(kTimeout));
        shared->returned.set();
    }));

    ASSERT_TRUE(shared->returned.wait());
    EXPECT_FALSE(shared->ended.load());
    EXPECT_EQ(source->waits(), 0U);
    EXPECT_FALSE(queue.submit([]() {
    }));
    EXPECT_TRUE(all_end(workers));
}
