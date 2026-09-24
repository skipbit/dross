#pragma once

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
 * - When the last handle goes, the queue stops accepting tasks. Its workers
 *   run what is left, both the queue's tasks and the tasks queued on their
 *   own loops, and then end on their own
 * - The destructor never waits for that, even the last one
 * - A timer installed on a worker's loop does not keep the worker alive; it
 *   goes when the worker ends
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
     * task to run. Returns false when the task is empty.
     */
    bool submit(std::function<void()> task);

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

    std::shared_ptr<storage> _store;
};

}  // namespace dross
