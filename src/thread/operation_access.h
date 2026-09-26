#pragma once

#include "dross/thread/operation.h"
#include "thread/time_source.h"

#include <any>
#include <memory>

// What the queue needs from an operation that the public interface does not
// offer.
namespace dross {

class operation_access final {
public:
    // An id no other call in this process has returned, greater than every
    // one returned before.
    static operation_id next_id() noexcept;

    // A result not yet filled in, for an operation with a new id. Its
    // wait_for() reads the time and waits through source.
    static operation_result make(std::shared_ptr<const time_source> source);

    // Fills in result with value, an empty one for an operation that returns
    // nothing, and wakes whatever waits for it. Called once per result.
    static void finish(const operation_result& result, std::any value);

    // Marks result cancelled, which counts as finished, and wakes whatever
    // waits for it. Called instead of finish(), never as well.
    static void cancel(const operation_result& result);
};

}  // namespace dross
