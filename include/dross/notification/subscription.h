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
 * - Once reset() or the destructor has returned, no new call to the
 *   function starts, including one already queued on a run loop or an
 *   operation_queue
 * - A call already running is not waited for and runs to its end, so either
 *   can be called from inside the function itself
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
     * @return false once this is empty, and once the run loop's thread has
     * ended or the operation_queue has stopped that the delivery names
     */
    bool active() const;

private:
    class storage;

    explicit subscription(std::shared_ptr<storage> store) noexcept;

    std::shared_ptr<storage> _store;

    friend class subscription_access;
};

}  // namespace dross
