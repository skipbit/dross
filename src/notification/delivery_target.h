#pragma once

#include "dross/thread/runloop.h"
#include "thread/operation_queue_access.h"

#include <variant>

// Where a subscription runs its function, once the subscribing thread is
// known. Used across the notification layer.
namespace dross {

// Run on the sending thread.
struct direct_target final { };

// Where a subscription runs its function.
using delivery_target = std::variant<direct_target, runloop, operation_queue_access::weak_queue>;

// Builds one visitor for std::visit from a function for each kind.
template <typename... F>
struct overloaded final : F... {
    using F::operator()...;
};

}  // namespace dross
