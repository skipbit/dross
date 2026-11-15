#pragma once

#include "dross/notification/delivery.h"
#include "notification/delivery_target.h"

// What a delivery names, once the subscribing thread is known. Used by the
// rest of the notification layer and by the library's own tests.
namespace dross {

class delivery_access final {
public:
    // Where which runs a function for a subscription made on the calling
    // thread: queued() without a loop names the calling thread's.
    static delivery_target resolve(const delivery& which);
};

}  // namespace dross
