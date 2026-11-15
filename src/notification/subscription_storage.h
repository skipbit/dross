#pragma once

#include "dross/notification/subscription.h"
#include "notification/delivery_access.h"

#include <atomic>
#include <functional>
#include <memory>

// The receiving end behind a subscription: the subscriber's function, where
// it runs, and whether the subscription has ended. What sends a notification
// holds one too, and hands each notification to deliver().
namespace dross {

class subscription::storage final : public std::enable_shared_from_this<storage> {
public:
    // The subscriber's function, given a pointer to the notification's value.
    using function_type = std::function<void(const void*)>;

    storage(delivery_target target, function_type function);

    // Runs the function for value the way target says: here and now for
    // direct_target, and otherwise as a task that holds value until it runs.
    // Every subscriber can be handed the same value. Does nothing once ended.
    void deliver(const std::shared_ptr<const void>& value);

    // Ends the subscription. A call already running is not waited for.
    void end() noexcept;

    bool active() const;

private:
    // Runs the function unless the subscription has ended by now, which is
    // what keeps a task queued before end() from calling it after.
    void call(const void* value) const;

    // Set once, at construction, and never reassigned, so read without a
    // lock.
    const delivery_target _target;
    const function_type _function;

    std::atomic<bool> _ended{ false };
};

}  // namespace dross
