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
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <utility>
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

// A type only the tests know, so taking it out of a result tells whether the
// type is recognised across the boundary of a shared library build.
struct answer {
    std::string text;
    int number;
};

static_assert(dross::operation_task_type<int (*)()>);
static_assert(dross::operation_task_type<void (*)()>);
static_assert(! dross::operation_task_type<void (*)(int)>);
static_assert(! dross::operation_task_type<std::unique_ptr<int> (*)()>);

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

TEST(operation_queue_test, enqueue_gives_back_what_the_task_returned)
{
    dross::operation_queue queue{ 2 };

    const auto number = queue.enqueue([]() {
        return 42;
    });
    const auto own_type = queue.enqueue([]() {
        return answer{ "life", 42 };
    });
    ASSERT_TRUE(number.has_value());
    ASSERT_TRUE(own_type.has_value());
    ASSERT_TRUE(number->wait_for(kTimeout));
    ASSERT_TRUE(own_type->wait_for(kTimeout));

    EXPECT_EQ(number->get_as<int>(), 42);
    const auto taken = own_type->get_as<answer>();
    ASSERT_TRUE(taken.has_value());
    EXPECT_EQ(taken->text, "life");
    EXPECT_EQ(taken->number, 42);
    const auto wrong = own_type->get_as<int>();
    ASSERT_FALSE(wrong.has_value());
    EXPECT_TRUE(wrong.error() == dross::operation_errc::type_mismatch);
}

TEST(operation_queue_test, enqueue_of_a_task_returning_nothing_is_read_as_void)
{
    dross::operation_queue queue{ 1 };
    auto ran = std::make_shared<std::atomic<bool>>(false);

    const auto result = queue.enqueue([ran]() {
        ran->store(true);
    });
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->wait_for(kTimeout));

    EXPECT_TRUE(ran->load());
    EXPECT_TRUE(result->get_as<void>().has_value());
}

TEST(operation_queue_test, an_enqueued_result_is_unfinished_while_its_task_runs)
{
    dross::operation_queue queue{ 1 };
    auto release = std::make_shared<event>();

    const auto result = queue.enqueue([release]() {
        release->wait();
        return 1;
    });
    ASSERT_TRUE(result.has_value());

    EXPECT_FALSE(result->is_finished());
    EXPECT_FALSE(queue.wait_for(std::chrono::milliseconds{ 1 }));
    const auto early = result->get_as<int>();
    ASSERT_FALSE(early.has_value());
    EXPECT_TRUE(early.error() == dross::operation_errc::not_finished);

    release->set();
    EXPECT_TRUE(queue.wait_for(kTimeout));
    EXPECT_TRUE(result->is_finished());
    EXPECT_EQ(result->get_as<int>(), 1);
}

TEST(operation_queue_test, enqueue_keeps_one_order_with_submit)
{
    struct state {
        event release;
        std::mutex mutex;
        std::vector<int> order;
    };
    auto shared = std::make_shared<state>();
    dross::operation_queue queue{ 1 };
    const auto record = [shared](int n) {
        const std::lock_guard<std::mutex> guard{ shared->mutex };
        shared->order.push_back(n);
    };

    // The first holds the only worker until every task is queued behind it.
    ASSERT_TRUE(queue.submit([shared]() {
        shared->release.wait();
    }));
    ASSERT_TRUE(queue.submit([record]() {
        record(0);
    }));
    ASSERT_TRUE(queue.enqueue([record]() {
        record(1);
    }));
    ASSERT_TRUE(queue.submit([record]() {
        record(2);
    }));
    const auto last = queue.enqueue([record]() {
        record(3);
    });
    ASSERT_TRUE(last.has_value());
    shared->release.set();

    ASSERT_TRUE(last->wait_for(kTimeout));
    const std::lock_guard<std::mutex> guard{ shared->mutex };
    EXPECT_EQ(shared->order, (std::vector<int>{ 0, 1, 2, 3 }));
}

TEST(operation_queue_test, enqueue_from_many_threads_gives_each_task_its_own_result)
{
    constexpr int kSubmitters = 4;
    constexpr int kPerSubmitter = 50;

    dross::operation_queue queue{ 3 };
    std::vector<std::vector<dross::operation_result>> results(kSubmitters);

    std::vector<std::thread> submitters;
    submitters.reserve(kSubmitters);
    for (int submitter = 0; submitter < kSubmitters; ++submitter) {
        submitters.emplace_back([queue, &mine = results[submitter], submitter]() mutable {
            for (int n = 0; n < kPerSubmitter; ++n) {
                const int value = (submitter * kPerSubmitter) + n;
                if (auto result = queue.enqueue([value]() {
                    return value;
                })) {
                    mine.push_back(*result);
                }
            }
        });
    }
    for (auto& submitter : submitters) {
        submitter.join();
    }

    std::unordered_set<dross::operation_id> ids;
    for (int submitter = 0; submitter < kSubmitters; ++submitter) {
        ASSERT_EQ(results[submitter].size(), static_cast<std::size_t>(kPerSubmitter));
        for (int n = 0; n < kPerSubmitter; ++n) {
            const auto& result = results[submitter][n];
            ASSERT_TRUE(result.wait_for(kTimeout));
            EXPECT_EQ(result.get_as<int>(), (submitter * kPerSubmitter) + n);
            ids.insert(result.id());
        }
    }
    EXPECT_EQ(ids.size(), static_cast<std::size_t>(kSubmitters * kPerSubmitter));
}

TEST(operation_queue_test, a_task_can_wait_for_the_result_of_one_it_enqueued)
{
    dross::operation_queue queue{ 2 };

    const auto outer = queue.enqueue([queue]() mutable {
        const auto inner = queue.enqueue([]() {
            return 7;
        });
        if ((! inner) || (! inner->wait_for(kTimeout))) {
            return -1;
        }
        return inner->get_as<int>().value_or(-1);
    });
    ASSERT_TRUE(outer.has_value());
    ASSERT_TRUE(outer->wait_for(kTimeout * 2));

    EXPECT_EQ(outer->get_as<int>(), 7);
}

TEST(operation_queue_test, enqueue_after_shutdown_gives_no_result)
{
    dross::operation_queue queue{ 1 };
    ASSERT_TRUE(queue.shutdown(kTimeout));

    const auto result = queue.enqueue([]() {
        return 1;
    });

    EXPECT_FALSE(result.has_value());
}

TEST(operation_queue_test, cancel_takes_a_waiting_task_off_the_queue)
{
    dross::operation_queue queue{ 1 };
    auto release = std::make_shared<event>();
    auto ran = std::make_shared<std::atomic<bool>>(false);

    // The first holds the only worker, so the next is still waiting.
    ASSERT_TRUE(queue.submit([release]() {
        release->wait();
    }));
    const auto waiting = queue.enqueue([ran]() {
        ran->store(true);
    });
    ASSERT_TRUE(waiting.has_value());

    EXPECT_TRUE(queue.cancel(waiting->id()));
    EXPECT_TRUE(waiting->is_finished());
    const auto value = waiting->get_as<void>();
    ASSERT_FALSE(value.has_value());
    EXPECT_TRUE(value.error() == dross::operation_errc::cancelled);

    // Anything the cancelled task would have done happens before a task
    // queued after it runs.
    release->set();
    const auto later = queue.enqueue([]() {
        return 1;
    });
    ASSERT_TRUE(later.has_value());
    ASSERT_TRUE(later->wait_for(kTimeout));
    EXPECT_FALSE(ran->load());
}

TEST(operation_queue_test, cancel_leaves_a_started_or_finished_task_alone)
{
    dross::operation_queue queue{ 1 };
    auto started = std::make_shared<event>();
    auto release = std::make_shared<event>();

    const auto result = queue.enqueue([started, release]() {
        started->set();
        release->wait();
        return 5;
    });
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(started->wait());

    EXPECT_FALSE(queue.cancel(result->id()));
    release->set();
    ASSERT_TRUE(result->wait_for(kTimeout));
    EXPECT_FALSE(queue.cancel(result->id()));
    EXPECT_EQ(result->get_as<int>(), 5);
}

TEST(operation_queue_test, cancel_succeeds_once)
{
    dross::operation_queue queue{ 1 };
    auto release = std::make_shared<event>();
    ASSERT_TRUE(queue.submit([release]() {
        release->wait();
    }));
    const auto waiting = queue.enqueue([]() {
        return 1;
    });
    ASSERT_TRUE(waiting.has_value());

    EXPECT_TRUE(queue.cancel(waiting->id()));
    EXPECT_FALSE(queue.cancel(waiting->id()));
    release->set();
}

TEST(operation_queue_test, cancel_ignores_an_id_from_another_queue)
{
    dross::operation_queue queue{ 1 };
    dross::operation_queue other{ 1 };
    auto release = std::make_shared<event>();
    ASSERT_TRUE(other.submit([release]() {
        release->wait();
    }));
    const auto elsewhere = other.enqueue([]() {
        return 2;
    });
    ASSERT_TRUE(elsewhere.has_value());

    EXPECT_FALSE(queue.cancel(elsewhere->id()));
    release->set();
    ASSERT_TRUE(elsewhere->wait_for(kTimeout));
    EXPECT_EQ(elsewhere->get_as<int>(), 2);
}

TEST(operation_queue_test, wait_for_stops_waiting_for_a_task_once_it_is_cancelled)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    dross::operation_queue queue = dross::operation_queue_access::make(1, source);
    auto release = std::make_shared<event>();
    auto blocked = std::make_shared<event>();

    // The only worker is held by work on its own loop rather than by a task
    // from the queue, so the next task waits with nothing else for the
    // waiter to wait for.
    const auto holder = queue.enqueue([release, blocked]() {
        dross::current_thread().perform([release, blocked]() {
            blocked->set();
            release->wait();
        });
    });
    ASSERT_TRUE(holder.has_value());
    ASSERT_TRUE(blocked->wait());
    const auto waiting = queue.enqueue([]() {
        return 1;
    });
    ASSERT_TRUE(waiting.has_value());

    // The waiter's own bound is longer than the test waits for it, so only
    // the cancel, not its deadline, ends the wait in time.
    std::atomic<bool> finished{ false };
    event returned;
    std::thread waiter{ [queue, &finished, &returned]() mutable {
        finished.store(queue.wait_for(kTimeout * 2));
        returned.set();
    } };
    const bool saw_wait = source->await_timed_waits(1);
    ASSERT_TRUE(queue.cancel(waiting->id()));
    const bool woke = returned.wait();
    release->set();
    waiter.join();

    EXPECT_TRUE(saw_wait);
    EXPECT_TRUE(woke);
    EXPECT_TRUE(finished.load());
}

TEST(operation_queue_test, wait_for_settles_when_the_last_earlier_task_is_cancelled_ahead_of_a_later_one)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    dross::operation_queue queue = dross::operation_queue_access::make(1, source);
    auto release = std::make_shared<event>();
    auto blocked = std::make_shared<event>();

    // The only worker is held by work on its own loop rather than by a task
    // from the queue, so the tasks behind it wait with nothing else for the
    // waiter to wait for. It is held for longer than the test waits for the
    // waiter, so the worker coming free cannot end the wait in its place.
    const auto holder = queue.enqueue([release, blocked]() {
        dross::current_thread().perform([release, blocked]() {
            blocked->set();
            release->wait(kTimeout * 3);
        });
    });
    ASSERT_TRUE(holder.has_value());
    ASSERT_TRUE(blocked->wait());
    const auto x = queue.enqueue([]() {
        return 10;
    });
    ASSERT_TRUE(x.has_value());

    // The waiter's own bound is longer than the test waits for it, so only
    // cancelling x, not its deadline, ends the wait in time.
    std::atomic<bool> finished{ false };
    event returned;
    std::thread waiter{ [queue, &finished, &returned]() mutable {
        finished.store(queue.wait_for(kTimeout * 2));
        returned.set();
    } };
    const bool saw_wait = source->await_timed_waits(1);
    // Accepted after the wait began, so y's order is the wait's target.
    // Cancelling x leaves y at the front of the list at exactly that order,
    // so only front().order >= target settles the wait at once;
    // front().order > target would wait out the deadline instead.
    const auto y = queue.enqueue([]() {
        return 20;
    });
    ASSERT_TRUE(y.has_value());

    ASSERT_TRUE(queue.cancel(x->id()));
    const bool woke = returned.wait();
    release->set();
    waiter.join();

    EXPECT_TRUE(saw_wait);
    EXPECT_TRUE(woke);
    EXPECT_TRUE(finished.load());
    ASSERT_TRUE(y->wait_for(kTimeout));
    EXPECT_EQ(y->get_as<int>(), 20);
}

TEST(operation_queue_test, wait_for_still_waits_for_an_earlier_task_when_a_later_one_is_cancelled)
{
    dross::operation_queue queue{ 1 };
    auto release_first = std::make_shared<event>();
    auto second_started = std::make_shared<event>();
    auto release_second = std::make_shared<event>();
    ASSERT_TRUE(queue.submit([release_first]() {
        release_first->wait();
    }));
    ASSERT_TRUE(queue.submit([second_started, release_second]() {
        second_started->set();
        release_second->wait();
    }));
    const auto third = queue.enqueue([]() {
        return 3;
    });
    ASSERT_TRUE(third.has_value());

    ASSERT_TRUE(queue.cancel(third->id()));
    const bool while_first_runs = queue.wait_for(std::chrono::milliseconds::zero());
    release_first->set();
    ASSERT_TRUE(second_started->wait());
    const bool while_second_runs = queue.wait_for(std::chrono::milliseconds::zero());
    release_second->set();

    EXPECT_FALSE(while_first_runs);
    EXPECT_FALSE(while_second_runs);
    EXPECT_TRUE(queue.wait_for(kTimeout));
}

TEST(operation_queue_test, a_cancelled_task_is_destroyed_where_its_captures_may_use_the_queue)
{
    // Submits to the queue when the last copy goes, as a scope guard might.
    struct submit_on_destroy final {
        dross::operation_queue queue;
        std::shared_ptr<event> submitted;

        ~submit_on_destroy()
        {
            if (queue.submit([]() {
            })) {
                submitted->set();
            }
        }
    };

    dross::operation_queue queue{ 1 };
    auto release = std::make_shared<event>();
    auto submitted = std::make_shared<event>();
    ASSERT_TRUE(queue.submit([release]() {
        release->wait();
    }));
    auto guard = std::make_shared<submit_on_destroy>(queue, submitted);
    const auto waiting = queue.enqueue([guard]() {
        return 1;
    });
    ASSERT_TRUE(waiting.has_value());
    guard.reset();

    // Cancelled on a thread of its own, so a cancel that never returns fails
    // this test instead of stopping the suite.
    auto returned = std::make_shared<event>();
    auto cancelled = std::make_shared<std::atomic<bool>>(false);
    std::thread canceller{ [queue, id = waiting->id(), returned, cancelled]() mutable {
        cancelled->store(queue.cancel(id));
        returned->set();
    } };
    if (! returned->wait()) {
        // Kept so the queue is never torn down under the stuck cancel.
        new dross::operation_queue{ queue };
        canceller.detach();
        release->set();
        FAIL() << "cancel() did not return";
    }
    canceller.join();
    release->set();

    EXPECT_TRUE(cancelled->load());
    EXPECT_TRUE(submitted->wait());
    EXPECT_TRUE(queue.wait_for(kTimeout));
}

TEST(operation_queue_test, a_task_is_copied_and_destroyed_only_outside_the_queues_lock)
{
    // Each time it is copied or destroyed, has another thread make a call
    // that takes the queue's lock, and counts the times that call does not
    // get through. Small enough, and copied without throwing, that a
    // std::function may keep it inline, and so copy it when it moves.
    struct lock_check final {
        struct counts final {
            dross::operation_queue queue;
            std::atomic<int> checked{ 0 };
            std::atomic<int> blocked{ 0 };
        };

        std::shared_ptr<counts> state;

        explicit lock_check(std::shared_ptr<counts> shared)
            : state{ std::move(shared) }
        {
        }

        lock_check(const lock_check& other) noexcept
            : state{ other.state }
        {
            check();
        }

        ~lock_check()
        {
            check();
        }

        void check() const noexcept
        {
            ++state->checked;
            // One is enough to fail; the rest would each wait out the bound.
            if (state->blocked.load() > 0) {
                return;
            }
            auto through = std::make_shared<event>();
            std::thread{ [queue = std::optional<dross::operation_queue>{ state->queue }, through]() mutable {
                queue->wait_for(std::chrono::milliseconds::zero());
                // Drops its handle before it signals, so no handle outlives
                // the check.
                queue.reset();
                through->set();
            } }.detach();
            if (! through->wait()) {
                ++state->blocked;
            }
        }
    };
    static_assert(std::is_nothrow_copy_constructible_v<lock_check>);

    dross::operation_queue queue{ 1 };
    auto release = std::make_shared<event>();
    ASSERT_TRUE(queue.submit([release]() {
        release->wait();
    }));
    const lock_check check{ std::make_shared<lock_check::counts>(queue) };
    ASSERT_TRUE(queue.submit([check]() {
    }));
    const auto returned = queue.enqueue([check]() {
        return 1;
    });
    const auto cancelled = queue.enqueue([check]() {
        return 2;
    });
    ASSERT_TRUE(returned.has_value());
    ASSERT_TRUE(cancelled.has_value());
    EXPECT_TRUE(queue.cancel(cancelled->id()));
    release->set();

    EXPECT_TRUE(queue.wait_for(kTimeout));
    EXPECT_EQ(returned->get_as<int>(), 1);
    EXPECT_GT(check.state->checked.load(), 0);
    EXPECT_EQ(check.state->blocked.load(), 0);
}

TEST(operation_queue_test, cancel_from_many_threads_while_the_workers_take_tasks)
{
    constexpr int kCancellers = 4;
    constexpr int kPerCanceller = 50;
    constexpr int kTotal = kCancellers * kPerCanceller;

    dross::operation_queue queue{ 2 };
    auto release = std::make_shared<event>();
    for (std::size_t n = 0; n < queue.thread_count(); ++n) {
        ASSERT_TRUE(queue.submit([release]() {
            release->wait();
        }));
    }
    auto ran = std::make_shared<std::atomic<int>>(0);
    std::vector<dross::operation_result> results;
    results.reserve(kTotal);
    for (int n = 0; n < kTotal; ++n) {
        const auto result = queue.enqueue([n, ran]() {
            ran->fetch_add(1);
            return n;
        });
        ASSERT_TRUE(result.has_value());
        results.push_back(*result);
    }

    // The workers start taking while the cancellers take off the same list,
    // so some tasks are cancelled and the rest run.
    std::vector<int> cancelled(kTotal, 0);
    std::vector<std::thread> cancellers;
    cancellers.reserve(kCancellers);
    release->set();
    for (int canceller = 0; canceller < kCancellers; ++canceller) {
        cancellers.emplace_back([queue, &results, &cancelled, canceller]() mutable {
            for (int n = canceller; n < kTotal; n += kCancellers) {
                cancelled[n] = queue.cancel(results[n].id()) ? 1 : 0;
            }
        });
    }
    for (auto& canceller : cancellers) {
        canceller.join();
    }

    for (int n = 0; n < kTotal; ++n) {
        ASSERT_TRUE(results[n].wait_for(kTimeout));
        const auto value = results[n].get_as<int>();
        if (cancelled[n] != 0) {
            ASSERT_FALSE(value.has_value());
            EXPECT_TRUE(value.error() == dross::operation_errc::cancelled);
        } else {
            EXPECT_EQ(value, n);
        }
    }
    // Shows no task both ran and was cancelled: the ones that were not
    // cancelled account for every increment of ran.
    int cancelled_count = 0;
    for (int n = 0; n < kTotal; ++n) {
        if (cancelled[n] != 0) {
            ++cancelled_count;
        }
    }
    EXPECT_EQ(ran->load(), kTotal - cancelled_count);
    EXPECT_TRUE(queue.wait_for(kTimeout));
}
