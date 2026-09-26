#pragma once

#include "dross/thread/operation.h"

// What the queue needs from an operation that the public interface does not
// offer.
namespace dross {

class operation_access final {
public:
    // An id no other call in this process has returned, greater than every
    // one returned before.
    static operation_id next_id() noexcept;
};

}  // namespace dross
