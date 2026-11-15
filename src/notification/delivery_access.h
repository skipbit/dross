#pragma once

#include "dross/notification/delivery.h"
#include "dross/thread/runloop.h"
#include "thread/operation_queue_access.h"

#include <variant>

// What a delivery names, once the subscribing thread is known. Used by the
// rest of the notification layer and by the library's own tests.
namespace dross {

// Run on the sending thread.
struct direct_target final { };

// Where a subscription runs its function.
using delivery_target = std::variant<direct_target, runloop, operation_queue_access::weak_queue>;

class delivery_access final {
public:
    // Where which runs a function for a subscription made on the calling
    // thread: queued() without a loop names the calling thread's.
    static delivery_target resolve(const delivery& which);
};

}  // namespace dross
