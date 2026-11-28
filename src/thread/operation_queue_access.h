#pragma once

#include "dross/thread/operation_queue.h"
#include "thread/time_source.h"

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>

// Ways to make, refer to and inspect a queue that the public interface does
// not offer. Used by the library's create(), its own tests and the
// notification layer.
namespace dross {

class operation_queue_access final {
public:
    // A queue like create(max_concurrency) makes, except that its own waits
    // read the time and wait through source.
    static std::expected<operation_queue, error> make(std::size_t max_concurrency, std::shared_ptr<const time_source> source);

    // A reference to a queue that does not keep it alive. lock() makes a
    // handle for as long as it is held, so a queue whose last other handle
    // goes meanwhile stops when that one does, on the thread that held it.
    using weak_queue = std::weak_ptr<operation_queue::storage>;

    static weak_queue weaken(const operation_queue& queue);

    // A handle to the queue, or std::nullopt once its last handle has gone.
    static std::optional<operation_queue> lock(const weak_queue& queue);
};

}  // namespace dross
