#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>

namespace dross {

/**
 * @brief A queue of tasks belonging to one thread, which that thread runs.
 *
 * A run loop holds tasks and runs them on the thread it belongs to. Any
 * thread may add a task; only the owning thread runs them. That is what lets
 * work be aimed at a particular thread, and what lets a result be handed
 * back to the thread it came from.
 *
 * Every thread has one run loop, reached through current_runloop(). The main
 * thread's is also reachable from anywhere through main_runloop(), so a
 * worker can post to it without being handed a copy first.
 *
 * Handle semantics:
 * - A runloop is a handle to a loop, not the loop itself. Copying gives
 *   another handle to the same loop
 * - operator== asks whether two handles name the same loop
 * - There is no empty handle and no null state. A handle keeps its loop
 *   alive, so posting to a loop whose thread has ended is well defined and
 *   reports false rather than reaching into freed memory
 * - Moving copies. The cost is one atomic increment, and it means a
 *   moved-from handle is still a usable handle
 *
 * Thread safety:
 * - Every operation is safe to call from any thread
 * - The running operations (run, run_one, run_pending, run_for) are meant
 *   for the owning thread. Running a loop from a thread it does not belong
 *   to runs the tasks on the wrong thread, which defeats the purpose
 *
 * Exceptions:
 * - An exception thrown by a task propagates out of the running call that
 *   was executing it. It is not caught, stored or translated, and the tasks
 *   behind it stay queued
 *
 * @code
 * // On a worker, handing a result back to the main thread.
 * runloop main_loop = main_runloop();
 *
 * std::thread worker{[main_loop]() mutable {
 *     auto answer = expensive_work();
 *     main_loop.perform([answer]() { show(answer); });
 * }};
 *
 * main_runloop().run();  // runs show(answer) here, until quit()
 * worker.join();
 * @endcode
 */
class runloop final {
public:
    /**
     * @brief Copy constructor, giving another handle to the same loop.
     * @param other The handle to copy
     */
    runloop(const runloop& other);

    /**
     * @brief Destructor.
     *
     * Destroying a handle does not stop the loop or discard its tasks.
     */
    ~runloop();

    /**
     * @brief Copy assignment, naming the same loop as the source.
     * @param other The handle to copy
     * @return Reference to this handle
     */
    runloop& operator=(const runloop& other);

    /**
     * @brief Add a task for the owning thread to run.
     * @param task The task, which must be callable
     * @return true when the task was queued
     *
     * The task is queued and this returns at once; it does not wait for the
     * task to run. Tasks run in the order they were added.
     *
     * Returns false when the task can never run:
     * - the owning thread has ended
     * - the task is empty
     *
     * A loop that is merely not running at the moment still accepts tasks,
     * because it may run later.
     */
    bool perform(std::function<void()> task);

    /**
     * @brief Run tasks until quit() is requested.
     * @return The number of tasks that ran
     *
     * Waits when no task is queued. Returns once quit() is seen, which
     * clears that request; tasks still queued stay queued.
     */
    std::size_t run();

    /**
     * @brief Run one task, waiting for one if the queue is empty.
     * @return true when a task ran, false when quit() ended the wait
     */
    bool run_one();

    /**
     * @brief Run the tasks already queued, without waiting.
     * @return The number of tasks that ran
     *
     * Only the tasks queued when the call started are run, so a task that
     * adds another does not keep this call going. A pending quit() request
     * neither stops this call nor is consumed by it.
     */
    std::size_t run_pending();

    /**
     * @brief Run tasks until the timeout passes or quit() is requested.
     * @param timeout How long to keep running
     * @return The number of tasks that ran
     *
     * A timeout of zero runs what is queued and returns without waiting.
     */
    std::size_t run_for(std::chrono::milliseconds timeout);

    /**
     * @brief Ask the running call to return.
     *
     * Stops the loop; it does not discard the queue. The request stays until
     * a running call sees it, so quitting a loop that is not running yet
     * ends its next run() rather than being lost. Use clear() to drop the
     * tasks.
     */
    void quit();

    /**
     * @brief Discard every queued task.
     *
     * Tasks already running are unaffected.
     */
    void clear();

    /**
     * @brief Count the queued tasks.
     * @return The number of tasks waiting to run
     */
    std::size_t pending_count() const;

    /**
     * @brief Test whether any task is queued.
     * @return true when nothing is waiting to run
     */
    bool empty() const;

    /**
     * @brief Test whether a running call is active on this loop.
     * @return true from inside a task, and while run() waits for one
     */
    bool is_running() const;

    /**
     * @brief Test whether two handles name the same loop.
     * @param other The handle to compare with
     * @return true when both name one loop
     */
    bool operator==(const runloop& other) const noexcept;

private:
    class storage;

    explicit runloop(std::shared_ptr<storage> store) noexcept;

    std::shared_ptr<storage> _store;

    friend runloop main_runloop();
    friend runloop current_runloop();
};

/**
 * @brief Get a handle to the main thread's run loop.
 * @return A handle to the loop belonging to the main thread
 *
 * Callable from any thread, which is what makes it the way back: a worker
 * posts to this loop and the main thread runs the task.
 *
 * The main thread is the one that loaded the library, recorded before main()
 * runs. A library loaded by a worker thread would record that thread
 * instead.
 */
runloop main_runloop();

/**
 * @brief Get a handle to the calling thread's run loop.
 * @return A handle to the loop belonging to this thread
 *
 * The loop is created on first use and belongs to the thread for as long as
 * the thread runs, whether or not dross started that thread. When the thread
 * ends, perform() on a handle to its loop reports false.
 */
runloop current_runloop();

}
