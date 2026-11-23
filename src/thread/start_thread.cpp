#include "thread/start_thread.h"

#include "thread/start_thread_access.h"

namespace dross {

namespace {

struct failures_to_come final {
    int after{ 0 };
    int count{ 0 };
};

// Each thread has its own plan, so a start on another thread neither takes
// a planned failure nor pays for a lock.
failures_to_come& planned()
{
    thread_local failures_to_come failures;
    return failures;
}

}  // namespace

bool take_planned_start_failure()
{
    auto& failures = planned();
    if (failures.count == 0) {
        return false;
    }
    if (failures.after > 0) {
        --failures.after;
        return false;
    }
    --failures.count;
    return true;
}

void start_thread_access::fail_next(const int count, const int after)
{
    auto& failures = planned();
    failures.count = count;
    failures.after = after;
}

}  // namespace dross
