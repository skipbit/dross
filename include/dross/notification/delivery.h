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
 *   call, or on that thread's run loop once the call has returned when the
 *   sender defers it; see dross::notify_options
 * - on() runs it on one of an operation_queue's workers
 *
 * Order, for notifications sent from one thread to one subscriber:
 * - With queued() and direct(), the function runs for them one at a time,
 *   in the order they were sent. For direct(), that holds among those sent
 *   with the same dross::notify_timing: a deferred one runs after any sent
 *   without deferring before its loop runs it
 * - With on(), they are taken from the queue in the order they were sent.
 *   On a queue with more than one worker, which of them the function starts
 *   on first is not fixed, and two of them can run at the same time, as two
 *   tasks submitted to the queue can, so a function given to on() is
 *   written to be safe for that. A queue with one worker keeps them in
 *   order and one at a time
 *
 * Notifications sent from different threads have no order between them.
 * With direct(), each runs the function on its own sending thread, so two of
 * them can run it at the same time, and a function given to direct() for a
 * topic sent from more than one thread is written to be safe for that.
 *
 * Lifetime:
 * - A subscription does not keep its loop's thread or its queue going. Once
 *   the thread has ended, or the queue has stopped, whether by shutdown()
 *   or by its last handle going, the subscription is no longer active and
 *   no new notification is handed on
 * - What was already queued follows the loop or the queue: a loop whose
 *   thread has ended drops it, and a queue that has stopped still runs it,
 *   as it runs every task it accepted. To have none of it run, end the
 *   subscription before stopping the queue
 * - So a queue made only to be passed to on() stops at the end of that
 *   statement, and the subscription is inactive from the start
 *
 * Handle semantics:
 * - A delivery is a small value that is copied freely, and moving copies
 *
 * @code
 * auto here = dross::delivery::queued();                          // the subscribing thread
 * auto on_main = dross::delivery::queued(dross::main_runloop());  // the main thread
 * auto in_place = dross::delivery::direct();                      // the sending thread
 *
 * auto workers = dross::operation_queue::create(4);
 * if (workers) {
 *     auto pooled = dross::delivery::on(*workers);  // one of the workers
 * }
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
     * @brief Run on the sending thread, inside the call that sends unless the
     * sender defers it.
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
