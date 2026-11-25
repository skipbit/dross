#pragma once

#include "dross/type/error.h"

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace dross {

/**
 * @brief A handle to an OS thread running its own run loop.
 *
 * dross::thread starts an OS thread and gives back a handle to it.
 * Every thread it starts runs its own run loop (see runloop), and so accepts
 * tasks through perform(). Two kinds of thread can be started:
 * - create() starts a thread that runs its loop until quit()
 * - create(body) starts a thread that runs body and then its loop, which
 *   ends once there is nothing left to do: no task is queued and no timer
 *   is installed. A task performed while body runs therefore runs after
 *   it, and a timer that keeps repeating keeps the thread until quit()
 *
 * body always runs to its end, even when quit() comes first; quit() then
 * stops the loop once body has returned. Either kind refuses tasks from the
 * moment its loop stops, so a task perform() accepts always runs unless
 * quit() drops it.
 *
 * A thread the library did not start can still be named: thread::main() names
 * the process's initial thread, and thread::current() adopts whichever thread
 * calls it. Both get a loop too, and so accept tasks, because their owner
 * may choose to drive it themselves.
 *
 * Both block until the new thread has published its run loop (if any) and
 * its native id, so perform() and native_id() work on the returned handle
 * at once, with no race. When the system will not start the new thread,
 * both return std::errc::resource_unavailable_try_again, and when there is
 * no memory to start it, std::errc::not_enough_memory.
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
 *   thread::current().stop_requested() and returning on its own
 * - quit() stops the thread's run loop once the task it is running
 *   returns; the thread then ends, dropping the tasks still queued
 *
 * Waiting:
 * - For a thread dross started, join() and join_for() wait until the OS
 *   thread itself has ended. For a thread dross only adopted, they wait
 *   until its bookkeeping in this library has been torn down
 * - Several handles may wait at once
 * - join_for() bounds the wait for that bookkeeping. What the OS thread
 *   still has to tear down after it is waited for without a bound, which
 *   is normally a brief moment
 * - Calling join() from the thread it names would deadlock forever, so it
 *   returns at once instead; join_for() does the same, for the same reason
 *
 * Thread safety:
 * - Every operation is free of data races when called from any thread
 *
 * @code
 * auto worker = dross::thread::create();
 * if (worker) {
 *     worker->perform([]() {
 *         if (dross::thread::current().stop_requested()) {
 *             return;
 *         }
 *         // ...
 *     });
 *     worker->cancel();
 *     worker->quit();
 *     worker->join_for(std::chrono::seconds{1});
 * }
 * @endcode
 */
class thread final {
public:
    /**
     * @brief Start a thread that runs its own run loop.
     * @return A handle to the new thread, or
     * std::errc::resource_unavailable_try_again when the system will not
     * start it, or std::errc::not_enough_memory when there is no memory to
     * start it
     *
     * Blocks until the new thread has published its run loop and native id.
     */
    [[nodiscard]] static std::expected<thread, error> create();

    /**
     * @brief Start a thread that runs body and ends once nothing is left.
     * @param body The function the new thread runs first
     * @return A handle to the new thread, or
     * std::errc::resource_unavailable_try_again when the system will not
     * start it, or std::errc::not_enough_memory when there is no memory to
     * start it
     *
     * body runs on the new thread before its run loop, and the thread ends
     * once body has returned, no task is queued and no timer is installed.
     * Blocks until the new thread has published its run loop and native id.
     */
    [[nodiscard]] static std::expected<thread, error> create(std::function<void()> body);

    /**
     * @brief Get a handle to the process's initial thread.
     * @return A handle to the main thread
     *
     * Callable from any thread. The main thread is the one the process
     * started on, as the system reports it, so which thread loaded the
     * library does not come into it.
     *
     * Marking this record finished, and dropping it from thread::all(),
     * happens through the main thread's own bookkeeping, which only the main
     * thread can install for itself. A process whose main thread never calls
     * thread::current() or thread::main() leaves this record never marked
     * finished, even after the process has exited.
     */
    static thread main();

    /**
     * @brief Get a handle to the calling thread.
     * @return A handle to this thread
     *
     * Adopts the calling thread on first use, whether or not dross started
     * it. The handle belongs to the thread for as long as it runs; once the
     * thread ends, perform() on a handle to it reports false.
     *
     * Defined even when called from a thread-local destructor that runs
     * after this thread's own bookkeeping has already been torn down, such
     * as a user's own thread-local destructor running after this library's
     * when the user's was constructed first: it then returns a handle to a
     * shared, already-finished record, the same one every such call gets,
     * rather than reaching into the destroyed one.
     */
    static thread current();

    /**
     * @brief Get handles to every thread that is alive right now.
     * @return Handles to the live threads, in no particular order
     *
     * Includes the main thread and any thread that has called
     * thread::current(), as well as every thread dross started. A thread
     * appears once it has been registered and disappears once it has ended.
     */
    static std::vector<thread> all();

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
     * Delegates to the thread's run loop. Returns false when the task is
     * empty, or once the thread's loop has stopped.
     */
    bool perform(std::function<void()> task);

    /**
     * @brief Stop this thread's run loop.
     *
     * Takes effect once the task the loop is running returns. A thread
     * dross started then ends and drops the tasks still queued.
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
     * For a thread dross started, waits until the OS thread itself has
     * ended. Several handles may join at once. Calling this from the thread
     * it names would deadlock forever, so it returns at once instead.
     */
    void join();

    /**
     * @brief Wait for this thread to end, for at most timeout.
     * @param timeout How long to wait
     * @return true when the thread has ended
     *
     * When it returns true, a thread dross started has ended as join()
     * waits for it to. The timeout bounds the wait for the thread's
     * bookkeeping in this library; what the OS thread still has to tear
     * down after that is waited for without a bound, which is normally a
     * brief moment. A timeout of zero waits only for that. Calling this
     * from the thread it names returns at once, for the same reason as
     * join().
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
    bool is_current() const;

    /**
     * @brief Get the operating system's identifier for this thread.
     * @return The id dross::native::thread_id() reported on this thread
     *
     * Always engaged, with one exception: thread::main() on a platform that
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

    friend class thread_access;
};

}  // namespace dross
