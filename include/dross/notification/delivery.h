#pragma once

#include "dross/thread/operation_queue.h"
#include "dross/thread/runloop.h"

#include <memory>

namespace dross {

/**
 * @brief Where and when a subscriber's function runs for each notification.
 *
 * There is no default: every subscription names one of the three.
 *
 * - queued() runs it on a run loop, never inside the call that sent the
 *   notification, even when that call is made on the loop's own thread.
 *   Without a loop, the loop is the subscribing thread's
 * - direct() runs it on the thread that sends the notification, inside that
 *   call
 * - on() runs it on one of an operation_queue's workers
 *
 * Order:
 * - Notifications sent from one thread to one subscriber start in the order
 *   they were sent
 * - With queued() and direct() each also finishes before the next starts.
 *   On a queue with more than one worker, two of them can run at the same
 *   time, as two tasks submitted to the queue can, so a function given to
 *   on() is written to be safe for that
 *
 * Lifetime:
 * - A subscription does not keep its loop's thread or its queue going. Once
 *   the thread has ended, or the queue has stopped, whether by shutdown()
 *   or by its last handle going, the subscription is no longer active and
 *   its function is no longer called
 * - So a queue made only to be passed to on() stops at the end of that
 *   statement, and the subscription is inactive from the start
 *
 * Handle semantics:
 * - A delivery is a small value that is copied freely, and moving copies
 *
 * @code
 * dross::operation_queue workers{ 4 };
 *
 * auto here = dross::delivery::queued();                          // the subscribing thread
 * auto on_main = dross::delivery::queued(dross::main_runloop());  // the main thread
 * auto in_place = dross::delivery::direct();                      // the sending thread
 * auto pooled = dross::delivery::on(workers);                     // one of the workers
 * @endcode
 */
class delivery final {
public:
    /**
     * @brief Run on the subscribing thread's run loop.
     * @return The delivery
     *
     * The loop is the one belonging to the thread that subscribes, which is
     * not necessarily the thread that calls this.
     */
    static delivery queued();

    /**
     * @brief Run on loop.
     * @param loop The loop to run on
     * @return The delivery
     */
    static delivery queued(runloop loop);

    /**
     * @brief Run on the sending thread, inside the call that sends.
     * @return The delivery
     */
    static delivery direct();

    /**
     * @brief Run on one of queue's workers.
     * @param queue The queue to run on, which the subscription does not keep
     * going
     * @return The delivery
     */
    static delivery on(const operation_queue& queue);

    /**
     * @brief Copy constructor.
     * @param other The delivery to copy
     */
    delivery(const delivery& other);

    /**
     * @brief Destructor.
     */
    ~delivery();

    /**
     * @brief Copy assignment.
     * @param other The delivery to copy
     * @return Reference to this delivery
     */
    delivery& operator=(const delivery& other);

private:
    class storage;

    explicit delivery(std::shared_ptr<const storage> store) noexcept;

    std::shared_ptr<const storage> _store;

    friend class delivery_access;
};

}  // namespace dross
