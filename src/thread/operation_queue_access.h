#pragma once

#include "dross/thread/operation_queue.h"
#include "thread/time_source.h"

#include <cstddef>
#include <memory>

// Ways to make a queue that the public interface does not offer. Used by the
// library's own tests.
namespace dross {

class operation_queue_access final {
public:
    // A queue like operation_queue(thread_count) makes, except that its own
    // waits read the time and wait through source.
    static operation_queue make(std::size_t thread_count, std::shared_ptr<const time_source> source);
};

}  // namespace dross
