#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>

namespace dross {

/**
 * @brief A fixed set of worker threads that run submitted tasks in the order
 * they were submitted.
 *
 * Where runloop and thread aim work at one particular thread, a queue hands
 * it to whichever of its workers is free. Each worker is a dross::thread
 * running its own run loop, so a task can use current_thread() and
 * current_runloop() as the way back to its worker, and install a timer on
 * it, the same as on any other thread dross started.
 *
 * Workers:
 * - The number of workers is given when the queue is made and never changes
 * - The tasks wait in one first-in, first-out list that every worker takes
 *   from, so they start in the order they were submitted; with more than one
 *   worker, they may finish in any order
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
 *   tasks. Its workers run what is left, both the queue's tasks and the
 *   tasks queued on their own loops, and then end on their own
 * - The destructor never waits for that, even the last one; shutdown() is
 *   the way to wait for it
 * - A timer installed on a worker's loop does not keep the worker alive; it
 *   goes when the worker ends
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
     * @return true when the task was queued
     *
     * The task is queued and this returns at once; it does not wait for the
     * task to run. Returns false when the task is empty, or once the queue
     * has been shut down.
     */
    bool submit(std::function<void()> task);

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

    std::shared_ptr<storage> _store;

    friend class operation_queue_access;
};

}  // namespace dross
