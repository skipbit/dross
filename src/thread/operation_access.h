#pragma once

#include "dross/thread/operation.h"
#include "thread/time_source.h"

#include <any>
#include <cstdint>
#include <memory>

// What the queue needs from an operation that the public interface does not
// offer.
namespace dross {

class operation_access final {
public:
    // An id no other call in this process has returned, greater than every
    // one returned before.
    static operation_id next_id() noexcept;

    // A result not yet filled in, for an operation with a new id, made by
    // the queue numbered queue, or by none for 0. Its wait_for() reads the
    // time and waits through source.
    static operation_result make(std::shared_ptr<const time_source> source, std::uint64_t queue = 0);

    // The number of the queue that made result, or 0 for none.
    static std::uint64_t queue_of(const operation_result& result) noexcept;

    // Whether result was cancelled, as opposed to not finished yet or
    // filled in.
    static bool is_cancelled(const operation_result& result);

    // Fills in result with value, an empty one for an operation that returns
    // nothing, and wakes whatever waits for it. Whichever of finish() and
    // cancel() is called first for a result takes effect; a later call to
    // either one changes nothing.
    static void finish(const operation_result& result, std::any value);

    // Marks result cancelled, which counts as finished, and wakes whatever
    // waits for it. Whichever of finish() and cancel() is called first for a
    // result takes effect; a later call to either one changes nothing.
    static void cancel(const operation_result& result);
};

}  // namespace dross
