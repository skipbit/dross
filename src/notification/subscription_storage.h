#pragma once

#include "dross/notification/subscription.h"
#include "notification/delivery_target.h"
#include "notification/notification_access.h"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

// The receiving end behind a subscription: the subscriber's function, where
// it runs, and whether the subscription has ended. What sends a notification
// holds one too, and hands each notification to deliver().
namespace dross {

class subscription::storage final : public std::enable_shared_from_this<storage> {
public:
    // The subscriber's function, given a pointer to the notification's value.
    using function_type = std::function<void(const void*)>;

    // One sending, which can be cancelled.
    using sending = notification_access::storage_type;

    // Where a subscription came from: for one made through a listener, the
    // listener, which it keeps going, and the broadcaster the listener hears
    // as its source; for one made on an observable, the observable as its
    // source. It does not keep its source going.
    struct origin final {
        std::shared_ptr<const void> listener;
        std::weak_ptr<const void> source;
        // Takes the subscription out of the source's list, when the source
        // still exists.
        std::function<void()> detach;
    };

    storage(delivery_target target, function_type function, std::optional<origin> from = std::nullopt);

    // Runs the function for value the way target says: here and now for
    // direct_target unless deferred, and otherwise as a task that holds value
    // until it runs, on the sending thread's loop for a deferred
    // direct_target. Every subscriber can be handed the same value and the
    // same sent. Does nothing once ended.
    void deliver(const std::shared_ptr<const void>& value, const std::shared_ptr<const sending>& sent, bool deferred);

    // Ends the subscription and detaches it from its broadcaster. A call
    // already running is not waited for.
    void end() noexcept;

    bool ended() const noexcept;

    // False once the function can never be called again: the subscription
    // has ended, its target no longer takes work, or its source has gone.
    bool active() const;

private:
    // The queued form of deliver(): a task that holds value until it runs.
    std::function<void()> make_task(const std::shared_ptr<const void>& value, const std::shared_ptr<const sending>& sent);

    // Runs the function unless the subscription has ended or sent has been
    // cancelled by now, which is what keeps a task queued before either from
    // calling it after.
    void call(const void* value, const sending& sent) const;

    // Set once, at construction, and never reassigned, so read without a
    // lock.
    const delivery_target _target;
    const function_type _function;
    const std::optional<origin> _from;

    std::atomic<bool> _ended{ false };
};

}  // namespace dross
