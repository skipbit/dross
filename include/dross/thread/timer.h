#pragma once

#include <dross/thread/runloop.h>

#include <chrono>
#include <functional>
#include <memory>

namespace dross {

/**
 * @brief A timer installed on a run loop, firing a callback once or on an
 * interval.
 *
 * A timer is its own class, not a method on runloop, the way NSTimer relates
 * to NSRunLoop: it names a scheduled callback, and the loop it is installed
 * on is what actually calls it.
 *
 * Handle semantics:
 * - A timer is a handle to an installed timer, not the timer itself.
 *   Copying gives another handle to the same timer
 * - operator== asks whether two handles name the same timer
 * - There is no empty handle and no null state. Moving copies, so a
 *   moved-from handle is still a usable handle
 *
 * Lifetime:
 * - The loop holds the timer once it is installed. Dropping every handle to
 *   a timer does not stop it; only invalidate() does
 * - invalidate() is callable from any thread, is idempotent, and makes
 *   valid() false at once. The loop drops its own reference to an
 *   invalidated timer
 * - A one-shot timer invalidates itself once it has fired
 * - A factory call with an empty callback, or a loop that has already
 *   finished, does not install anything: it returns a handle that is
 *   already invalid, the same as one invalidate() has already been called on
 *
 * Firing:
 * - repeating() installs a timer that fires every interval until
 *   invalidated; once() fires a single time and then invalidates itself
 * - The callback receives the timer itself, so it can invalidate() from
 *   inside to stop a repeating timer, or just read its own state
 * - The two-argument overloads install on current_runloop(); the
 *   three-argument overloads install on the loop given
 * - An interval of zero is allowed and means "due immediately"; a repeating
 *   timer with a zero interval then fires once per pass the loop makes over
 *   what is due, not once per unit of work it runs; see runloop::run()'s own
 *   doc comment for what a pass is
 * - After a repeating timer fires, its next fire is scheduled interval after
 *   the moment it fired, not stacked onto the fire that was due: a callback
 *   that runs long, or a loop that is not run for a while, does not make up
 *   the fires it missed
 * - No reading or writing the scheduled time; there is nothing like a
 *   tolerance
 *
 * Exceptions:
 * - An exception thrown by the callback propagates out of the running call
 *   that was executing it, the same way a task's does. A repeating timer
 *   stays installed; a one-shot does not, since it is removed before it
 *   fires regardless of whether the callback throws
 *
 * @code
 * dross::timer heartbeat = dross::timer::repeating(
 *     std::chrono::milliseconds{100},
 *     [count = 0](dross::timer self) mutable {
 *         if (++count >= 10) {
 *             self.invalidate();
 *         }
 *     });
 * @endcode
 */
class timer final {
public:
    /**
     * @brief Install a timer that fires every interval on current_runloop().
     * @param interval How long to wait between fires; zero means "every
     * iteration"
     * @param callback Called with the timer itself each time it fires
     * @return A handle to the installed timer, or a handle that is already
     * invalid when callback is empty
     */
    static timer repeating(std::chrono::milliseconds interval, std::function<void(timer)> callback);

    /**
     * @brief Install a timer that fires every interval on the given loop.
     * @param interval How long to wait between fires; zero means "every
     * iteration"
     * @param callback Called with the timer itself each time it fires
     * @param loop The loop to install on
     * @return A handle to the installed timer, or a handle that is already
     * invalid when callback is empty or loop has already finished
     */
    static timer repeating(std::chrono::milliseconds interval, std::function<void(timer)> callback, runloop loop);

    /**
     * @brief Install a timer that fires once on current_runloop().
     * @param delay How long to wait before firing; zero means "immediately"
     * @param callback Called with the timer itself when it fires
     * @return A handle to the installed timer, or a handle that is already
     * invalid when callback is empty
     */
    static timer once(std::chrono::milliseconds delay, std::function<void(timer)> callback);

    /**
     * @brief Install a timer that fires once on the given loop.
     * @param delay How long to wait before firing; zero means "immediately"
     * @param callback Called with the timer itself when it fires
     * @param loop The loop to install on
     * @return A handle to the installed timer, or a handle that is already
     * invalid when callback is empty or loop has already finished
     */
    static timer once(std::chrono::milliseconds delay, std::function<void(timer)> callback, runloop loop);

    /**
     * @brief Copy constructor, giving another handle to the same timer.
     * @param other The handle to copy
     */
    timer(const timer& other);

    /**
     * @brief Destructor.
     *
     * Destroying a handle does not stop the timer; see invalidate().
     */
    ~timer();

    /**
     * @brief Copy assignment, naming the same timer as the source.
     * @param other The handle to copy
     * @return Reference to this handle
     */
    timer& operator=(const timer& other);

    /**
     * @brief Stop this timer and drop it from its loop.
     *
     * Callable from any thread and idempotent. Makes valid() false at once.
     * A fire the loop has already taken, whether about to run or already
     * running, is not interrupted and runs to completion; what this
     * guarantees is that no new fire is taken after it returns.
     */
    void invalidate();

    /**
     * @brief Test whether this timer is still installed and may still fire.
     * @return false once invalidate() has been called, or a one-shot timer
     * has already fired
     */
    bool valid() const;

    /**
     * @brief Test whether this timer fires more than once.
     * @return true for a timer made with repeating(), false for once()
     */
    bool repeats() const;

    /**
     * @brief Get the spacing this timer was installed with.
     * @return The interval for a repeating timer, or the delay a once()
     * timer was given before it fired
     */
    std::chrono::milliseconds interval() const;

    /**
     * @brief Test whether two handles name the same timer.
     * @param other The handle to compare with
     * @return true when both name one timer
     */
    bool operator==(const timer& other) const noexcept;

private:
    class storage;

    explicit timer(std::shared_ptr<storage> store) noexcept;

    static timer make(std::chrono::milliseconds interval, bool repeats, std::function<void(timer)> callback, runloop loop);

    std::shared_ptr<storage> _store;

    friend class runloop;
};

}  // namespace dross
