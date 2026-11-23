#include "thread/start_thread.h"

#include "thread/start_thread_access.h"

#include <new>
#include <system_error>
#include <utility>

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

bool take_failure()
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

}  // namespace

std::expected<std::thread, error> start_thread(std::function<void()> body)
{
    if (take_failure()) {
        return std::unexpected(error{ std::errc::resource_unavailable_try_again });
    }
    // std::thread reports the failure only by throwing. It starts the thread
    // with the default attributes, for which the system fails only for want
    // of resources.
    try {
        return std::thread{ std::move(body) };
    } catch (const std::system_error&) {
        return std::unexpected(error{ std::errc::resource_unavailable_try_again });
    } catch (const std::bad_alloc&) {
        return std::unexpected(error{ std::errc::not_enough_memory });
    }
}

void start_thread_access::fail_next(const int count, const int after)
{
    auto& failures = planned();
    failures.count = count;
    failures.after = after;
}

}  // namespace dross
