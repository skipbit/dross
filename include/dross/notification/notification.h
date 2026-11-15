#pragma once

#include <memory>

namespace dross {

/**
 * @brief One notification, as broadcaster::notify() sent it.
 *
 * notify() returns one for every call. It names that one sending, not the
 * kind of notification, which is what a dross::topic names. Keeping it is
 * optional; most callers drop it.
 *
 * Handle semantics:
 * - A notification is a handle. Copying gives another handle to the same
 *   sending, and cancel() through any of them cancels that sending
 *
 * Cancelling:
 * - Once cancel() has returned, a delivery of the sending still queued on a
 *   run loop or an operation_queue no longer calls the subscriber's
 *   function, and on the thread that cancelled, nothing does
 * - It does not wait for a call on another thread. That call runs to its
 *   end, and one that was about to start there can still enter the function
 *   just after cancel() returns, as with subscription::reset()
 * - What has already run is not undone. A dross::delivery::direct()
 *   subscriber has already been called by the time notify() returns, unless
 *   the sending was deferred; see dross::notify_options
 * - Calling it again, from any thread or from inside a subscriber's
 *   function, does nothing more. On a sending no one was subscribed to, it
 *   does nothing
 * - A notify() that leaves by an exception returns none, so what it had
 *   already handed on cannot be cancelled
 */
class notification final {
public:
    /**
     * @brief Copy constructor, giving another handle to the same sending.
     * @param other The handle to copy
     */
    notification(const notification& other);

    /**
     * @brief Destructor.
     */
    ~notification();

    /**
     * @brief Copy assignment, naming the same sending as the source.
     * @param other The handle to copy
     * @return Reference to this handle
     */
    notification& operator=(const notification& other);

    /**
     * @brief Stop every delivery of this sending that has not started.
     */
    void cancel() noexcept;

private:
    class storage;

    explicit notification(std::shared_ptr<storage> store) noexcept;

    std::shared_ptr<storage> _store;

    friend class broadcaster;
    friend class notification_access;
};

}  // namespace dross
