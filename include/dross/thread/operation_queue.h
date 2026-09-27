#pragma once

#include "dross/thread/operation.h"

#include <any>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace dross {

/**
 * @brief A task operation_queue::enqueue() takes: callable with no arguments,
 * copyable, and returning nothing or a value operation_result can give back.
 */
template <typename F>
concept operation_task_type = std::copy_constructible<std::decay_t<F>> && std::invocable<std::decay_t<F>&>
                              && operation_value_type<std::decay_t<std::invoke_result_t<std::decay_t<F>&>>>;

/**
 * @brief How a queue runs a task given to operation_queue::submit() or
 * operation_queue::enqueue().
 *
 * Every member has a default, so a task given no options gets them all, and
 * one given some can name just those:
 * @code
 * queue.enqueue(render, { .priority = dross::operation_priority::high });
 * queue.enqueue(save, { .after = { *loaded, *checked } });
 * @endcode
 */
struct operation_options final {
    operation_priority priority{ operation_priority::normal };  ///< How soon it runs, next to the others waiting
    std::vector<operation_result> after{};                      ///< Results of tasks on the same queue to return before it runs
};

/**
 * @brief A fixed set of worker threads that run submitted tasks, the higher
 * priority first and otherwise in the order they were submitted.
 *
 * Where runloop and thread aim work at one particular thread, a queue hands
 * it to whichever of its workers is free. Each worker is a dross::thread
 * running its own run loop, so a task can use current_thread() and
 * current_runloop() as the way back to its worker, and install a timer on
 * it, the same as on any other thread dross started.
 *
 * Workers:
 * - The number of workers is given when the queue is made and never changes
 * - The tasks wait in one list that every worker takes from. A worker takes
 *   the task of the highest operation_priority waiting, and of those, the
 *   one submitted first, so tasks of one priority start in the order they
 *   were submitted; with more than one worker, they may finish in any order
 *
 * Running after others:
 * - A task given results in operation_options::after starts only once
 *   every one of them has returned; until then it waits aside, and its
 *   priority orders it only from then on
 * - A result that has already returned by the time the task is submitted
 *   counts as returned
 * - Only a result from the same queue can be named, and only once its task
 *   has been accepted, so tasks cannot run after one another in a circle
 * - When one of them is cancelled, whether before the task is submitted or
 *   after, the task is cancelled too and never runs, and so is every task
 *   that runs after it in turn. A task given to submit() that is cancelled
 *   this way is dropped without a word, as it has no result to report it
 *
 * Handle semantics:
 * - An operation_queue is a handle to a queue, not the queue itself.
 *   Copying gives another handle to the same queue
 * - operator== asks whether two handles name the same queue
 * - There is no empty handle and no null state, so moving copies; a
 *   moved-from handle is still a usable handle naming the same queue
 *
 * Lifetime:
 * - When the last handle goes, or on shutdown(), the queue stops accepting
 *   tasks. Its workers run what is left, both the queue's tasks and what
 *   those tasks queued on their own worker's loop, and then end on their
 *   own
 * - A task another thread queues on a worker's loop once the queue has
 *   stopped may not run, the same as one queued on any dross::thread whose
 *   loop is quitting
 * - The destructor never waits for that, even the last one; shutdown() is
 *   the way to wait for it
 * - A timer installed on a worker's loop does not keep the worker alive; it
 *   goes when the worker ends
 * - quit() on a worker's thread, such as current_thread().quit() from a
 *   task, ends that worker early. A queue whose workers have all ended that
 *   way still accepts tasks but never runs them
 *
 * Waiting:
 * - wait_for() waits for the tasks already submitted to finish, while the
 *   queue goes on accepting more
 * - shutdown() stops the queue accepting tasks and waits for its workers to
 *   end
 * - Called from one of the queue's own workers, either returns at once
 *   instead, since the task it is called from can never finish while it
 *   waits
 *
 * Thread safety:
 * - Every operation is free of data races when called from any thread,
 *   including from a task running on one of the queue's own workers
 *
 * Exceptions:
 * - An exception thrown by a task propagates out of the worker's loop, the
 *   same as a task given to thread::perform(). It is not caught, stored or
 *   translated
 *
 * @code
 * dross::operation_queue queue{ 4 };
 * dross::runloop main_loop = dross::main_runloop();
 * queue.submit([main_loop]() mutable {
 *     auto answer = expensive_work();
 *     main_loop.perform([answer]() { show(answer); });
 * });
 * @endcode
 */
class operation_queue final {
public:
    /**
     * @brief Start a queue with thread_count workers.
     * @param thread_count The number of workers, at least one
     *
     * Blocks until every worker has started.
     *
     * Throws std::invalid_argument when thread_count is zero, and propagates
     * std::system_error when the system will not start a worker; the workers
     * already started then end without running anything.
     */
    explicit operation_queue(std::size_t thread_count);

    /**
     * @brief Copy constructor, giving another handle to the same queue.
     * @param other The handle to copy
     */
    operation_queue(const operation_queue& other);

    /**
     * @brief Destructor.
     *
     * Destroying the last handle stops the queue accepting tasks, without
     * waiting for its workers to end.
     */
    ~operation_queue();

    /**
     * @brief Copy assignment, naming the same queue as the source.
     * @param other The handle to copy
     * @return Reference to this handle
     */
    operation_queue& operator=(const operation_queue& other);

    /**
     * @brief Add a task for one of the workers to run.
     * @param task The task, which must be callable
     * @param options How to run it
     * @return true when the task was queued
     *
     * The task is queued and this returns at once; it does not wait for the
     * task to run. Returns false when the task is empty, when its priority is
     * not one operation_priority names, when options.after names a result
     * from another queue, or once the queue has been shut down.
     */
    bool submit(std::function<void()> task, operation_options options = {});

    /**
     * @brief Add a task for one of the workers to run, and get a handle to
     * what it returns.
     * @param task The task
     * @param options How to run it
     * @return The result, not yet filled in, or the reason the task was not
     * queued: operation_errc::invalid_priority for a priority
     * operation_priority does not name, operation_errc::foreign_dependency
     * when options.after names a result from another queue, or
     * operation_errc::queue_stopped once the queue has stopped
     *
     * Queued in the same list as submit(), so the two keep one order. Returns
     * at once, as submit() does; the result is filled in when the task
     * returns. A task that throws propagates the same as one given to
     * submit().
     */
    template <operation_task_type F>
    std::expected<operation_result, error> enqueue(F&& task, operation_options options = {});

    /**
     * @brief Take a task given to enqueue() off the queue before it starts.
     * @param id The id of the task's result
     * @return true when the task was still waiting and has been taken off
     *
     * A task taken off never runs. Its result counts as finished and reports
     * operation_errc::cancelled, whatever waits for it wakes, and wait_for()
     * no longer waits for it. Every task that runs after it is taken off the
     * same way, and every task that runs after those in turn. A task still
     * waiting for others to return counts as not started. Returns false,
     * and changes nothing, once the task has started or been cancelled, and
     * for an id from another queue. A task that has started is never
     * stopped.
     */
    bool cancel(const operation_id& id);

    /**
     * @brief Wait for the tasks submitted so far to finish, for at most
     * timeout.
     * @param timeout How long to wait
     * @return true when every task submitted before this call has finished
     *
     * Tasks submitted after the call starts are not waited for. A timeout of
     * zero does not wait; it reports whether they have already finished.
     * Called from one of the queue's own workers, this does not wait either,
     * for the same reason as thread::join().
     */
    bool wait_for(std::chrono::milliseconds timeout);

    /**
     * @brief Stop accepting tasks and wait for the workers to end, for at
     * most timeout.
     * @param timeout How long to wait
     * @return true when every worker has ended
     *
     * The workers run what was already submitted before they end. The queue
     * stops accepting tasks even when this returns false, and its workers
     * still end once they are done. A timeout of zero does not wait; it
     * reports whether they have already ended. Called from one of the
     * queue's own workers, this does not wait either, for the same reason as
     * thread::join().
     */
    bool shutdown(std::chrono::milliseconds timeout);

    /**
     * @brief Get the number of workers.
     * @return The count the queue was made with
     */
    std::size_t thread_count() const noexcept;

    /**
     * @brief Test whether two handles name the same queue.
     * @param other The handle to compare with
     * @return true when both name one queue
     */
    bool operator==(const operation_queue& other) const noexcept;

private:
    class storage;

    explicit operation_queue(std::shared_ptr<storage> store) noexcept;

    // enqueue() without its type: task returns what the task returned, or
    // an empty std::any for one that returns nothing.
    std::expected<operation_result, error> enqueue_any(std::function<std::any()> task, operation_options options);

    std::shared_ptr<storage> _store;

    friend class operation_queue_access;
};

template <operation_task_type F>
std::expected<operation_result, error> operation_queue::enqueue(F&& task, operation_options options)
{
    using value_type = std::decay_t<std::invoke_result_t<std::decay_t<F>&>>;

    // Wrapped here, not in the library, so the value is put in on the same
    // side of a shared library boundary as get_as() takes it out.
    return enqueue_any([task = std::forward<F>(task)]() mutable {
        if constexpr (std::is_void_v<value_type>) {
            task();
            return std::any{};
        } else {
            return std::make_any<value_type>(task());
        }
    }, std::move(options));
}

}  // namespace dross
