#pragma once

#include "dross/notification/subscription.h"
#include "notification/delivery_target.h"

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

    // Where a subscription made through a listener came from: the listener,
    // which it keeps going, and the broadcaster the listener hears, which it
    // does not.
    struct origin final {
        std::shared_ptr<const void> listener;
        std::weak_ptr<const void> broadcaster;
    };

    storage(delivery_target target, function_type function, std::optional<origin> from = std::nullopt);

    // Runs the function for value the way target says: here and now for
    // direct_target, and otherwise as a task that holds value until it runs.
    // Every subscriber can be handed the same value. Does nothing once ended.
    void deliver(const std::shared_ptr<const void>& value);

    // Ends the subscription. A call already running is not waited for.
    void end() noexcept;

    bool ended() const noexcept;

    // False once the function can never be called again: the subscription
    // has ended, its target no longer takes work, or the broadcaster it came
    // from has gone.
    bool active() const;

private:
    // The queued form of deliver(): a task that holds value until it runs.
    std::function<void()> make_task(const std::shared_ptr<const void>& value);

    // Runs the function unless the subscription has ended by now, which is
    // what keeps a task queued before end() from calling it after.
    void call(const void* value) const;

    // Set once, at construction, and never reassigned, so read without a
    // lock.
    const delivery_target _target;
    const function_type _function;
    const std::optional<origin> _from;

    std::atomic<bool> _ended{ false };
};

}  // namespace dross
