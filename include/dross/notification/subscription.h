#pragma once

#include <memory>

namespace dross {

/**
 * @brief The right to go on receiving, held by the subscriber.
 *
 * Subscribing returns one. While it is held and active, the subscriber's
 * function is called for each notification, the way its dross::delivery
 * says. Destroying it or calling reset() ends that.
 *
 * Ending:
 * - Once reset() or the destructor has returned, a notification still
 *   queued on a run loop or an operation_queue no longer calls the function,
 *   and on the thread that ended it, nothing does
 * - Neither waits for a call on another thread. That call runs to its end,
 *   and one that was about to start there can still enter the function just
 *   after either returns. What the function uses is therefore held by the
 *   function, captured by value or through a std::shared_ptr, rather than
 *   referred to from outside it
 * - Not waiting is also what lets either be called from inside the function
 *   itself
 *
 * Handle semantics:
 * - A subscription has one holder. It moves and does not copy, as
 *   std::unique_ptr does, unlike the handles of the thread module, which
 *   share what they name
 * - A default-constructed one, a moved-from one and one after reset() are
 *   empty, and not active
 *
 * Exceptions:
 * - An exception thrown by the function is not caught. It propagates out of
 *   the call that ran the function: the sending call for
 *   dross::delivery::direct(), and the running call of the run loop or the
 *   worker otherwise
 */
class subscription final {
public:
    /**
     * @brief Construct an empty subscription, not active.
     */
    subscription() noexcept;

    /**
     * @brief Move constructor, leaving other empty.
     * @param other The subscription to take over
     */
    subscription(subscription&& other) noexcept;

    /**
     * @brief Destructor, ending the subscription as reset() does.
     */
    ~subscription();

    /**
     * @brief Move assignment, ending the subscription this held first.
     * @param other The subscription to take over
     * @return Reference to this subscription
     */
    subscription& operator=(subscription&& other) noexcept;

    subscription(const subscription& other) = delete;
    subscription& operator=(const subscription& other) = delete;

    /**
     * @brief End the subscription, leaving this empty.
     *
     * Does nothing on an empty one.
     */
    void reset() noexcept;

    /**
     * @brief Test whether the function can still be called.
     * @return false once this is empty, once the run loop's thread has ended
     * or the operation_queue has stopped that the delivery names, and once
     * the broadcaster subscribed to has gone
     *
     * Once false, it stays false: the function is never called again.
     */
    bool active() const;

private:
    class storage;

    explicit subscription(std::shared_ptr<storage> store) noexcept;

    std::shared_ptr<storage> _store;

    friend class subscription_access;
};

}  // namespace dross
