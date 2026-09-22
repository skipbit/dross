#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>

namespace dross {

class timer;

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
 * Timers (see dross::timer) install onto a loop the same way tasks are
 * queued onto it, and are the loop's other source of work: waiting stops at
 * whichever comes first, a queued task, an installed timer becoming due, or
 * quit(). A timer that is due fires before a queued task; see run()'s own
 * doc comment.
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
 * - Every operation is free of data races when called from any thread
 * - The running operations (run, run_one, run_pending, run_for) are for the
 *   thread the loop belongs to; a run already in progress on a loop whose
 *   owning thread has ended is not woken and keeps waiting
 *
 * Cleanup:
 * - When the thread that installed this loop's own bookkeeping ends, its
 *   queued tasks and installed timers are dropped, so captures held by
 *   either are released on that same thread. This happens only for a thread
 *   that installed its own bookkeeping, which for the main thread means one
 *   that has called current_runloop() or main_runloop() itself; see
 *   main_runloop()'s own doc comment for what that leaves unhandled
 *
 * Exceptions:
 * - An exception thrown by a task or a timer's callback propagates out of
 *   the running call that was executing it. It is not caught, stored or
 *   translated; the tasks behind it stay queued and a timer that threw
 *   stays installed
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
     * @brief Run tasks and due timers until quit() is requested.
     * @return The number of tasks run plus timer fires
     *
     * Waits when nothing is ready: no task is queued and no installed timer
     * has reached its deadline. The wait ends at whichever comes first, a
     * task being queued, a timer becoming due, or quit().
     *
     * Work is taken in passes. Within one pass, every timer already due
     * when the pass began fires once, earliest deadline first, before a
     * queued task runs; a timer that becomes due again before the pass
     * ends, such as one with a zero interval, does not get a second turn
     * until the next pass. Taking a task ends the pass; the next one starts
     * fresh. This is what stops a timer that is always due from starving
     * both the other timers and the queue.
     *
     * Returns once quit() is seen; tasks still queued stay queued. The
     * request is cleared by the outermost running call, so a run started
     * from inside a task stops but leaves the request standing for the run
     * it was started from.
     */
    std::size_t run();

    /**
     * @brief Run one task or due timer, waiting for one if neither is ready.
     * @return true when a task ran or a timer fired, false when quit() is
     * seen
     *
     * A pending quit() ends this at once, even with a task already queued
     * or a timer already due; that task or timer stays for next time.
     * Clearing the request follows the same rule as run().
     */
    bool run_one();

    /**
     * @brief Run the tasks queued and the timers due, without waiting.
     * @return The number of tasks run plus timer fires
     *
     * Only what was already queued or already due when the call started is
     * run, so a task that adds another, or a timer whose fire reschedules
     * it, does not keep this call going. This is the timer analogue of "the
     * tasks already queued": a due timer fires once here, a repeating one
     * is not chased through every interval it missed while nothing ran it.
     * A pending quit() request neither stops this call nor is consumed by
     * it.
     */
    std::size_t run_pending();

    /**
     * @brief Run tasks and due timers until the timeout passes or quit() is
     * requested.
     * @param timeout How long to keep running
     * @return The number of tasks run plus timer fires
     *
     * Work is taken in passes the same way run() takes it; see its own doc
     * comment. A timeout of zero runs nothing already due and returns at
     * once.
     */
    std::size_t run_for(std::chrono::milliseconds timeout);

    /**
     * @brief Ask the running call to return.
     *
     * Stops the loop; it does not discard the queue. The request stays until
     * the outermost running call sees it, so quitting a loop that is not
     * running yet ends its next run() rather than being lost. Use clear()
     * to drop the tasks.
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
     *
     * Speaks only of the task queue: a loop with no queued tasks can still
     * have work ahead of it in the form of installed timers; see
     * timer_count().
     */
    bool empty() const;

    /**
     * @brief Count the timers currently installed on this loop.
     * @return The number of timers in this loop's own bookkeeping
     *
     * This is the public way to observe that a timer::repeating() or
     * timer::once() call reached this loop. Can differ from a timer's own
     * valid(): a one-shot is dropped from this count before its callback
     * runs, while its valid() does not become false until after that
     * callback returns.
     */
    std::size_t timer_count() const;

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
    friend class timer;
};

/**
 * @brief Get a handle to the main thread's run loop.
 * @return A handle to the loop belonging to the main thread
 *
 * Callable from any thread, which is what makes it the way back: a worker
 * posts to this loop and the main thread runs the task.
 *
 * The main thread is the one the process started on, as the system reports
 * it, so which thread loaded the library does not come into it.
 *
 * Marking this loop finished, and releasing its queued tasks and installed
 * timers, happens through the main thread's own bookkeeping, which only the
 * main thread can install for itself. A process whose main thread never
 * calls current_runloop() or main_runloop() leaves this loop never marked
 * finished, its queued tasks never released, and any timer installed on it
 * from elsewhere sitting uncleaned, even after the process has exited; see
 * runloop's own "Cleanup" doc note. Nothing here fires such a timer either,
 * since that still requires something to run the loop.
 */
runloop main_runloop();

/**
 * @brief Get a handle to the calling thread's run loop.
 * @return A handle to the loop belonging to this thread
 *
 * The loop is created on first use and belongs to the thread for as long as
 * the thread runs, whether or not dross started that thread. When the thread
 * ends, perform() on a handle to its loop reports false.
 *
 * Defined even when called from a thread-local destructor that runs after
 * this thread's own bookkeeping has already been torn down, such as a
 * user's own thread-local destructor running after this library's when the
 * user's was constructed first: it then returns a handle to a shared,
 * already-finished loop, the same one every such call gets, rather than
 * reaching into the destroyed one.
 */
runloop current_runloop();

}
