#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace dross {

/**
 * @brief A handle to an OS thread, running either a run loop or a one-shot
 * body.
 *
 * dross::thread starts a detached OS thread and gives back a handle to it.
 * Two kinds of thread can be started:
 * - thread() starts a thread that runs its own run loop (see runloop), which
 *   this library drives by calling run() on it, and so accepts tasks
 *   through perform() for as long as it runs
 * - thread(body) starts a thread that runs body once and then ends; nothing
 *   drives a loop for it, so perform() on it always returns false
 *
 * A thread the library did not start can still be named: main_thread() names
 * the process's initial thread, and current_thread() adopts whichever thread
 * calls it. Both get a loop too, and so accept tasks, because their owner
 * may choose to drive it themselves.
 *
 * Both constructors block until the new thread has published its run loop
 * (if any) and its native id, so perform() and native_id() work on the
 * returned handle at once, with no race.
 *
 * Handle semantics:
 * - A thread is a handle to an OS thread, not the thread itself. Copying
 *   gives another handle to the same thread
 * - operator== asks whether two handles name the same thread
 * - There is no empty handle and no null state, so moving copies; a
 *   moved-from handle is still a usable handle naming the same thread
 * - The destructor does nothing to the thread: it neither stops nor joins
 *   it, and nothing happens to the thread automatically at process exit
 *
 * Cancellation:
 * - cancel() only raises a stop request that a body polls; it does not
 *   touch the thread's run loop. A body observes it by polling
 *   current_thread().stop_requested() and returning on its own
 * - quit() stops the thread's run loop, if it has one, keeping its queued
 *   tasks; the loop's own clear() is what drops them
 *
 * Waiting:
 * - join() and join_for() wait on a flag, not on the OS thread, so several
 *   handles may wait at once and the wait can be bounded
 * - Calling join() from the thread it names would deadlock forever, so it
 *   returns at once instead; join_for() does the same, for the same reason
 *
 * Thread safety:
 * - Every operation is free of data races when called from any thread
 *
 * Exceptions:
 * - Both constructors propagate std::system_error when the system will not
 *   start the new thread. It is not caught, stored or translated
 *
 * @code
 * dross::thread worker;
 * worker.perform([]() {
 *     if (dross::current_thread().stop_requested()) {
 *         return;
 *     }
 *     // ...
 * });
 * worker.cancel();
 * worker.quit();
 * worker.join_for(std::chrono::seconds{1});
 * @endcode
 */
class thread final {
public:
    /**
     * @brief Start a thread that runs its own run loop.
     *
     * Blocks until the new thread has published its run loop and native id.
     */
    thread();

    /**
     * @brief Start a thread that runs body once and then ends.
     * @param body The function the new thread runs
     *
     * The thread does not run a loop, so perform() on it always returns
     * false. Blocks until the new thread has published its native id.
     */
    explicit thread(std::function<void()> body);

    /**
     * @brief Copy constructor, giving another handle to the same thread.
     * @param other The handle to copy
     */
    thread(const thread& other);

    /**
     * @brief Destructor.
     *
     * Destroying a handle does not stop or join the thread.
     */
    ~thread();

    /**
     * @brief Copy assignment, naming the same thread as the source.
     * @param other The handle to copy
     * @return Reference to this handle
     */
    thread& operator=(const thread& other);

    /**
     * @brief Add a task for this thread to run.
     * @param task The task, which must be callable
     * @return true when the task was queued
     *
     * Delegates to the thread's run loop. Returns false when the thread has
     * no run loop (a one-shot thread), the task is empty, or the thread has
     * ended.
     */
    bool perform(std::function<void()> task);

    /**
     * @brief Stop this thread's run loop, keeping its queued tasks.
     *
     * Has no effect on a one-shot thread, which has no run loop to stop.
     */
    void quit();

    /**
     * @brief Request that this thread stop.
     *
     * Only records the request; it does not stop the run loop or the
     * thread. A body observes the request with stop_requested() and returns
     * on its own.
     */
    void cancel();

    /**
     * @brief Wait for this thread to end.
     *
     * Waits on a flag rather than the OS thread, so several handles may
     * join at once. Calling this from the thread it names would deadlock
     * forever, so it returns at once instead.
     */
    void join();

    /**
     * @brief Wait for this thread to end, for at most timeout.
     * @param timeout How long to wait
     * @return true when the thread has ended
     *
     * A timeout of zero does not wait; it reports whether the thread has
     * already ended. Calling this from the thread it names returns at once,
     * for the same reason as join().
     */
    bool join_for(std::chrono::milliseconds timeout);

    /**
     * @brief Test whether this thread has not yet ended.
     * @return true from the moment the thread starts until it ends
     */
    bool running() const;

    /**
     * @brief Test whether this thread has ended.
     * @return The opposite of running()
     */
    bool finished() const;

    /**
     * @brief Test whether cancel() has been requested.
     * @return true once cancel() has been called on this thread
     */
    bool stop_requested() const;

    /**
     * @brief Test whether the caller is the thread this handle names.
     * @return true when called from that thread
     */
    bool is_current_thread() const;

    /**
     * @brief Get the operating system's identifier for this thread.
     * @return The id dross::native::thread_id() reported on this thread
     *
     * Always engaged, with one exception: main_thread() on a platform that
     * cannot ask about another thread's id (macOS) reports empty until the
     * main thread has used this module itself, directly or by starting a
     * thread.
     */
    std::optional<std::uint64_t> native_id() const;

    /**
     * @brief Test whether two handles name the same thread.
     * @param other The handle to compare with
     * @return true when both name one thread
     */
    bool operator==(const thread& other) const noexcept;

private:
    class storage;

    explicit thread(std::shared_ptr<storage> store) noexcept;

    std::shared_ptr<storage> _store;

    friend thread main_thread();
    friend thread current_thread();
    friend std::vector<thread> all_threads();
};

/**
 * @brief Get a handle to the process's initial thread.
 * @return A handle to the main thread
 *
 * Callable from any thread. The main thread is the one the process started
 * on, as the system reports it, so which thread loaded the library does not
 * come into it.
 *
 * Marking this record finished, and dropping it from all_threads(), happens
 * through the main thread's own bookkeeping, which only the main thread can
 * install for itself. A process whose main thread never calls
 * current_thread() or main_thread() leaves this record never marked
 * finished, even after the process has exited.
 */
thread main_thread();

/**
 * @brief Get a handle to the calling thread.
 * @return A handle to this thread
 *
 * Adopts the calling thread on first use, whether or not dross started it.
 * The handle belongs to the thread for as long as it runs; once the thread
 * ends, perform() on a handle to it reports false.
 *
 * Defined even when called from a thread-local destructor that runs after
 * this thread's own bookkeeping has already been torn down, such as a
 * user's own thread-local destructor running after this library's when the
 * user's was constructed first: it then returns a handle to a shared,
 * already-finished record, the same one every such call gets, rather than
 * reaching into the destroyed one.
 */
thread current_thread();

/**
 * @brief Get handles to every thread that is alive right now.
 * @return Handles to the live threads, in no particular order
 *
 * Includes the main thread and any thread that has called current_thread(),
 * as well as every thread dross started. A thread appears once it has been
 * registered and disappears once it has ended.
 */
std::vector<thread> all_threads();

}  // namespace dross
